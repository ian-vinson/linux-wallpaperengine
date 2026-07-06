#pragma once

#include "Adapters/VectorAdapter.h"
#include "ConsoleObject.h"
#include "EngineObject.h"
#include "InputObject.h"
#include "LocalStorageObject.h"
#include "Modules/ScriptModule.h"
#include "SceneObject.h"

#include <chrono>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Model/Types.h"
#include "WallpaperEngine/Media/MediaSource.h"

namespace WallpaperEngine::Media {
class MediaSource;
}
extern "C" {
#include "quickjs.h"
}

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Scripting {
class ScriptPropertiesObject;
namespace Adapters {
    class ScriptableObjectAdapter;
}
using namespace WallpaperEngine::Data::Model;

// Opaque handle returned by createLayerScript. 0 means invalid / not created.
using ScriptLayerHandle = int;
static constexpr ScriptLayerHandle kInvalidLayerHandle = 0;

class ScriptEngine {
public:
    struct LoadedModule {
	DynamicValue& value;
	JSValue module;
	ScriptableObject& object;
    };
    struct JSObjectAdapters {
	std::unique_ptr<Adapters::VectorAdapter<4>> vec4;
	std::unique_ptr<Adapters::VectorAdapter<3>> vec3;
	std::unique_ptr<Adapters::VectorAdapter<2>> vec2;
	std::unique_ptr<Adapters::ScriptableObjectAdapter> object;
    };

    ~ScriptEngine ();
    ScriptEngine (Render::Wallpapers::CScene& scene, Media::MediaSource& mediaSource);
    ScriptEngine (const ScriptEngine&) = delete;
    ScriptEngine& operator= (const ScriptEngine&) = delete;

    JSRuntime* getRuntime () const { return m_runtime; }
    JSContext* getContext () const { return m_context; }
    JSValue getGlobalThis () const { return m_globalThis; }
    LoadedModule* getRunningModule () const { return m_runningModule; }
    JSValue dynamicToJs (DynamicValue& value) const;
    void jsToDynamic (JSValue val, DynamicValue& dst) const;

    /**
     * Evaluate a WallpaperEngine script's update() function.
     *
     * @param key The full JS script text (ES6 module with export function update(value))
     * @param currentValue The current value to pass to update()
     * @return The modified value from update(), or a copy of currentValue on error
     */
    void queueScript (const std::string& key, DynamicValue& currentValue, ScriptableObject& object);

    /**
     * Runs the deferred init()/update() priming call for every property script registered via
     * queueScript() since the last call to this method, in the order they were registered.
     *
     * queueScript() only compiles and registers a script — it does not run init()/update() —
     * specifically so that callers can construct every object in a scene first, then call this
     * once all of them exist. Scripts routinely resolve other layers by name via
     * thisScene.getLayer("someName") inside init(), and that only works if the referenced object
     * has already been constructed; running init() immediately per-object (interleaved with
     * construction) would make that fail for any name declared later in scene.json.
     *
     * Safe to call again if a script's own init()/update() ends up registering further scripts
     * (e.g. a dynamically created layer) — any newly-queued keys queued during a call are picked
     * up by that same call rather than left stranded.
     */
    void runPendingInits ();

    /**
     * Dispatches applyUserProperties() to every loaded script module with ONLY the changed
     * properties (not the full set — unlike the load-time call in runPendingInits(), which
     * intentionally sends everything as "changed"). No-op for scripts that don't export
     * applyUserProperties.
     */
    void notifyUserPropertiesChanged (const std::map<std::string, PropertySharedPtr>& changed);

    /**
     * Erases every property-script entry belonging to the given object from m_scriptModules,
     * freeing each entry's JS module value first. Must be called before the object itself is
     * deleted (as part of CScene::destroyObject()'s teardown) — tick() dereferences every
     * m_scriptModules entry's object reference unconditionally, every frame, so a stale entry
     * left behind would crash/UB on the very next tick() regardless of any JS-side reference.
     */
    void forgetObject (const ScriptableObject& object);

    /**
     * Runs a frame tick in the javascript engine. Dispatches any pending events,
     * timeouts, intervals AND calls any update() functions.
     */
    void tick ();

    // -------------------------------------------------------------------
    // Layer-script API (Phase 2 — dynamic text)
    // -------------------------------------------------------------------
    //
    // Wallpaper Engine text-object scripts follow a lifecycle pattern that
    // cannot be evaluated with the simple `update(value) -> value` contract
    // above. They typically look like:
    //
    //   'use strict';
    //   export var scriptProperties = createScriptProperties()…finish();
    //   export function init()   { /* subscribe to events, cache data    */ }
    //   export function update() { thisLayer.text = computeCurrentText(); }
    //
    // The script mutates a `thisLayer` object in place instead of returning
    // a value, and lifecycle functions are optional. The API below keeps
    // per-layer state alive across frames so `init()` runs once and
    // `update()` re-runs every tick.

    /**
     * Create a persistent "layer script" from a WE text-object script.
     *
     * @param scriptSource The full JS script text.
     * @param initialScriptProps Initial values for scriptProperties entries.
     *        Ownership stays with the caller; we only snapshot current values.
     * @param initialText Initial value of `thisLayer.text` (usually the
     *        static placeholder carried in the JSON).
     * @return A positive handle, or kInvalidLayerHandle if evaluation failed.
     */
    ScriptLayerHandle createLayerScript (
	const std::string& scriptSource, std::map<std::string, UserSettingUniquePtr>& initialScriptProps,
	const std::string& initialText
    );

    /**
     * Advance a layer by one frame.
     *
     * On the first call, invokes `init()` (if defined) before `update()`.
     * Updates a `thisScene` context visible to the script (time, fps).
     * Silently no-ops if the handle is invalid.
     */
    void tickLayer (ScriptLayerHandle handle, double time, double deltaTime, double fps);

    /**
     * Read the current value of `thisLayer.text` for the given layer.
     * Returns an empty string if the handle is invalid.
     */
    std::string layerText (ScriptLayerHandle handle);

    /**
     * Tear down a layer: invokes `destroy()` (if defined) and frees state.
     */
    void destroyLayer (ScriptLayerHandle handle);

    const JSObjectAdapters& getAdapters () const { return m_adapters; }
    const Render::Wallpapers::CScene& getScene () const { return m_scene; }
    const std::map<std::string, std::unique_ptr<Modules::ScriptModule>>& getModules () const { return m_modules; }

private:
    JSValue call (JSValue module, int argc, JSValueConst argv[], const char* name);

    void installBuiltins ();

    void notifyMediaUpdate (const Media::MediaSource::MediaInfo& media);

    // Installs globalThis.__layers and related helpers. Called lazily.
    void ensureLayerRegistry ();

    // Cursor event dispatch (cursorDown/cursorUp/cursorMove/cursorClick), called once per tick().
    void dispatchCursorEvents ();
    JSValue buildCursorEvent (const glm::vec3& worldPosition, const glm::vec2& screenPosition, bool leftDown);

    // Builds the changedUserProperties object passed to applyUserProperties().
    // Currently always contains every project-level user property — lwe has
    // no live property-reload path yet, only this load-time call (which real
    // WE also makes once on startup in addition to live edits).
    JSValue buildUserPropertiesObject () const;
    // Same as above, but built from an explicit changed-properties map instead of the full
    // project property set — used by notifyUserPropertiesChanged() for live reloads.
    JSValue buildUserPropertiesObject (const std::map<std::string, PropertySharedPtr>& changed) const;
    void dispatchCursorEvent (
	LoadedModule& module, const char* name, const glm::vec3& worldPosition, const glm::vec2& screenPosition,
	bool leftDown
    );

    JSRuntime* m_runtime = nullptr;
    JSContext* m_context = nullptr;
    JSValue m_globalThis;
    Render::Wallpapers::CScene& m_scene;
    std::unique_ptr<EngineObject> m_engineObject;
    std::unique_ptr<InputObject> m_inputObject;
    std::unique_ptr<SceneObject> m_sceneObject;
    std::unique_ptr<ConsoleObject> m_consoleObject;
    std::unique_ptr<ScriptPropertiesObject> m_scriptPropertiesObject;
    std::unique_ptr<LocalStorageObject> m_localStorageObject;

    std::map<std::string, std::unique_ptr<Modules::ScriptModule>> m_modules = {};
    std::map<std::string, LoadedModule> m_scriptModules = {};

    // Keys registered by queueScript() awaiting their deferred init()/update() priming call, in
    // registration order (m_scriptModules is a std::map, keyed and thus ordered by name, not
    // insertion order — this is what lets runPendingInits() preserve scene.json object order).
    std::vector<std::string> m_pendingInitKeys;

    LoadedModule* m_runningModule = nullptr;

    ScriptLayerHandle m_nextLayerId = 1;
    bool m_layerRegistryReady = false;
    std::map<ScriptLayerHandle, bool> m_layerInitialized;
    bool m_builtinsInstalled = false;
    Media::MediaSource& m_mediaSource;
    std::function<void ()> m_unregisterMediaUpdateCallback;
    std::function<void ()> m_unregisterAlbumArtUpdateCallback;

    bool m_cursorLeftDownLast = false;
    glm::vec3 m_cursorWorldPositionLast = {};
    glm::vec2 m_cursorScreenPositionLast = {};

    JSObjectAdapters m_adapters;
};
} // namespace WallpaperEngine::Scripting
