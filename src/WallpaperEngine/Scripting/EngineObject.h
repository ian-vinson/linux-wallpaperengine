#pragma once
#include "quickjs.h"

#include <chrono>
#include <map>
#include <vector>

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}
namespace WallpaperEngine::Scripting {
class ScriptEngine;
class EngineObject {
public:
    EngineObject (ScriptEngine& engine, Render::Wallpapers::CScene& scene);
    ~EngineObject ();

    const Render::Wallpapers::CScene& getScene () const { return m_scene; }
    JSValue getInstance () const { return m_instance; }
    ScriptEngine& getEngine () const { return m_engine; }
    uint32_t getInstanceId () const { return m_instanceId; }
    uint32_t reserveNextTimeoutId (JSValue function, uint64_t duration);
    uint32_t reserveNextIntervalId (JSValue function, uint64_t duration);
    void clearTimeout (uint32_t id);
    void clearInterval (uint32_t id);
    /**
     * Implements engine.registerAudioBuffers(resolution): returns an object whose
     * "average" array is refreshed every tick() with the current per-band audio
     * magnitude data (the same data driving the g_AudioSpectrumNN shader uniforms).
     *
     * @param resolution Requested band count; snapped to the nearest supported
     * tier (16, 32 or 64)
     */
    JSValue registerAudioBuffers (int32_t resolution);

    void tick ();

protected:
    struct Timeout {
	JSValue callback;
	std::chrono::milliseconds duration;
	std::chrono::steady_clock::time_point next;
    };

    struct AudioBufferBinding {
	/** Band count for this binding: always 16, 32 or 64 */
	uint32_t resolution;
	/** The JS array exposed as `average`, refreshed in place every tick() */
	JSValue average;
    };

    uint32_t m_nextTimeoutId = 0;
    uint32_t m_nextIntervalId = 0;
    std::map<uint32_t, Timeout> m_intervals;
    std::map<uint32_t, Timeout> m_timeouts;
    std::vector<AudioBufferBinding> m_audioBuffers;
    Render::Wallpapers::CScene& m_scene;
    ScriptEngine& m_engine;

    uint32_t m_instanceId;
    JSClassID m_classId;
    JSClassDef m_definition;
    JSValue m_instance;
};
}