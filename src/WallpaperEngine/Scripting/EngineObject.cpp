#include "EngineObject.h"
#include "ScriptEngine.h"
#include "WallpaperEngine/Audio/AudioContext.h"
#include "WallpaperEngine/Audio/Drivers/Recorders/PlaybackRecorder.h"
#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Camera.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"

using namespace WallpaperEngine::Scripting;

extern float g_Time;
extern float g_TimeLast;
extern float g_Daytime;

static uint32_t EngineInstanceId = 0;
std::map<uint32_t, EngineObject&> engineInstances;

JSValue engine_set_value (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    return JS_ThrowTypeError (ctx, "Cannot assign to read-only property");
}

JSValue engine_open_user_shortcut (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    return JS_UNDEFINED;
}

JSValue engine_get_frametime (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    return JS_NewFloat64 (ctx, g_Time - g_TimeLast);
}

JSValue engine_get_runtime (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    return JS_NewFloat64 (ctx, g_Time);
}

JSValue engine_get_daytime (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    return JS_NewFloat64 (ctx, g_Daytime);
}

// screenResolution/canvasSize read the live scene size on every access (rather than caching it)
// so a resize is reflected immediately, matching how engine_is_portrait reads CScene directly.
JSValue engine_get_screen_resolution (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    auto* engine = static_cast<EngineObject*> (JS_GetAnyOpaque (this_val, &classId));

    if (engine == nullptr) {
	return JS_ThrowTypeError (ctx, "invalid receiver for screenResolution");
    }

    const auto& scene = engine->getScene ();
    DynamicValue value (glm::vec2 (scene.getWidth (), scene.getHeight ()));

    return engine->getEngine ().getAdapters ().vec2->instantiate (value, true);
}

// canvasSize is documented as 2D-scenes-only (WE_DOCS_REFERENCE.md:242, lib.sceneScript.d.ts:2477).
// This fork's only signal for "2D scene" is an orthogonal camera projection (set from the scene's
// general.orthogonalprojection setting) -- 3D/perspective scenes leave the camera non-orthogonal,
// so canvasSize is undefined there, same as real Wallpaper Engine restricts it to 2D scenes.
JSValue engine_get_canvas_size (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    auto* engine = static_cast<EngineObject*> (JS_GetAnyOpaque (this_val, &classId));

    if (engine == nullptr) {
	return JS_ThrowTypeError (ctx, "invalid receiver for canvasSize");
    }

    const auto& scene = engine->getScene ();

    if (!scene.getCamera ().isOrthogonal ()) {
	return JS_UNDEFINED;
    }

    DynamicValue value (glm::vec2 (scene.getWidth (), scene.getHeight ()));

    return engine->getEngine ().getAdapters ().vec2->instantiate (value, true);
}

JSValue engine_stop_interval (
    JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic, JSValueConst* func_data
) {
    if (argc != 1) {
	return JS_EXCEPTION;
    }

    const auto it = engineInstances.find (magic);

    if (it == engineInstances.end ()) {
	return JS_EXCEPTION;
    }

    int id = 0;

    JS_ToInt32 (ctx, &id, argv[0]);

    it->second.clearInterval (id);

    return JS_UNDEFINED;
}

JSValue engine_stop_timeout (
    JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic, JSValueConst* func_data
) {
    if (argc != 1) {
	return JS_EXCEPTION;
    }

    const auto it = engineInstances.find (magic);

    if (it == engineInstances.end ()) {
	return JS_EXCEPTION;
    }

    int id = 0;

    JS_ToInt32 (ctx, &id, argv[0]);

    it->second.clearTimeout (id);

    return JS_UNDEFINED;
}

// setTimeout/setInterval now return a callable cancel closure (see makeCancelClosure below)
// rather than a raw id, matching real Wallpaper Engine's documented setTimeout/setInterval
// return type. clearTimeout/clearInterval remain as this fork's own extension for scripts
// still using the old-style `engine.clearTimeout(t)` call: the closure carries its id in a
// hidden, non-enumerable "__id" property so that old-style call keeps working unchanged.
static bool getCancelClosureId (JSContext* ctx, JSValueConst value, int* id) {
    if (!JS_IsObject (value)) {
	return false;
    }

    JSValue idProp = JS_GetPropertyStr (ctx, value, "__id");
    const bool found = !JS_IsUndefined (idProp);

    if (found) {
	JS_ToInt32 (ctx, id, idProp);
    }

    JS_FreeValue (ctx, idProp);
    return found;
}

JSValue engine_clear_interval (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    if (argc != 1) {
	return JS_ThrowTypeError (ctx, "clearInterval() expects exactly 1 argument, got %d", argc);
    }

    const auto it = engineInstances.find (magic);

    if (it == engineInstances.end ()) {
	return JS_ThrowReferenceError (ctx, "Could not find engine instance '%d' for clearInterval", magic);
    }

    int id = 0;

    if (!getCancelClosureId (ctx, argv[0], &id)) {
	JS_ToInt32 (ctx, &id, argv[0]);
    }

    it->second.clearInterval (id);
    return JS_UNDEFINED;
}

JSValue engine_clear_timeout (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    if (argc != 1) {
	return JS_ThrowTypeError (ctx, "clearTimeout() expects exactly 1 argument, got %d", argc);
    }

    const auto it = engineInstances.find (magic);

    if (it == engineInstances.end ()) {
	return JS_ThrowReferenceError (ctx, "Could not find engine instance '%d' for clearTimeout", magic);
    }

    int id = 0;

    if (!getCancelClosureId (ctx, argv[0], &id)) {
	JS_ToInt32 (ctx, &id, argv[0]);
    }

    it->second.clearTimeout (id);
    return JS_UNDEFINED;
}

JSValue engine_is_screensaver (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    return JS_FALSE;
}

JSValue engine_is_running_in_editor (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    return JS_FALSE;
}

// lwe only ever runs as a wallpaper (no screensaver mode exists, per engine_is_screensaver above).
JSValue engine_is_wallpaper (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    return JS_TRUE;
}

JSValue engine_is_portrait (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    const auto it = engineInstances.find (magic);

    if (it == engineInstances.end ()) {
	return JS_ThrowReferenceError (ctx, "Could not find engine instance '%d' for isPortrait", magic);
    }

    const auto& scene = it->second.getScene ();
    return JS_NewBool (ctx, scene.getHeight () > scene.getWidth ());
}

JSValue engine_cancel_interval (
    JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic, JSValueConst* func_data
) {
    int instanceId = 0;
    int id = 0;
    JS_ToInt32 (ctx, &instanceId, func_data[0]);
    JS_ToInt32 (ctx, &id, func_data[1]);

    const auto it = engineInstances.find (instanceId);

    if (it != engineInstances.end ()) {
	it->second.clearInterval (id);
    }

    return JS_UNDEFINED;
}

JSValue engine_cancel_timeout (
    JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic, JSValueConst* func_data
) {
    int instanceId = 0;
    int id = 0;
    JS_ToInt32 (ctx, &instanceId, func_data[0]);
    JS_ToInt32 (ctx, &id, func_data[1]);

    const auto it = engineInstances.find (instanceId);

    if (it != engineInstances.end ()) {
	it->second.clearTimeout (id);
    }

    return JS_UNDEFINED;
}

// builds the Function that setInterval/setTimeout return to script: calling it cancels the
// pending interval/timeout, matching real WE's documented behavior (lib.sceneScript.d.ts).
// instanceId/id are plain QuickJS ints (immediate values, not heap-allocated), so they need no
// JS_DupValue/JS_FreeValue bookkeeping despite being handed to JS_NewCFunctionData as JSValues.
static JSValue makeCancelClosure (JSContext* ctx, uint32_t instanceId, uint32_t id, JSCFunctionData* cancelFn) {
    JSValueConst boundData[]
	= { JS_NewInt32 (ctx, static_cast<int32_t> (instanceId)), JS_NewInt32 (ctx, static_cast<int32_t> (id)) };
    JSValue closure = JS_NewCFunctionData (ctx, cancelFn, 0, 0, 2, boundData);

    JS_DefinePropertyValueStr (ctx, closure, "__id", JS_NewInt32 (ctx, static_cast<int32_t> (id)), 0);
    return closure;
}

JSValue engine_set_interval (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    if (argc < 1) {
	return JS_ThrowTypeError (ctx, "setInterval() requires at least 1 argument (callback)");
    }

    int delay = 0;

    if (argc > 1) {
	JS_ToInt32 (ctx, &delay, argv[1]);
    }

    JSValue function = argv[0];

    if (!JS_IsFunction (ctx, function)) {
	return JS_ThrowTypeError (ctx, "setInterval() argument 1 must be a function");
    }

    const auto it = engineInstances.find (magic);

    if (it == engineInstances.end ()) {
	return JS_ThrowReferenceError (ctx, "Could not find engine instance '%d' for setInterval", magic);
    }

    uint32_t id = it->second.reserveNextIntervalId (function, delay);
    return makeCancelClosure (ctx, magic, id, engine_cancel_interval);
}

JSValue engine_set_timeout (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    if (argc < 1) {
	return JS_ThrowTypeError (ctx, "setTimeout() requires at least 1 argument (callback)");
    }

    int delay = 0;

    if (argc > 1) {
	JS_ToInt32 (ctx, &delay, argv[1]);
    }

    JSValue function = argv[0];

    if (!JS_IsFunction (ctx, function)) {
	return JS_ThrowTypeError (ctx, "setTimeout() argument 1 must be a function");
    }

    const auto it = engineInstances.find (magic);

    if (it == engineInstances.end ()) {
	return JS_ThrowReferenceError (ctx, "Could not find engine instance '%d' for setTimeout", magic);
    }

    uint32_t id = it->second.reserveNextTimeoutId (function, delay);
    return makeCancelClosure (ctx, magic, id, engine_cancel_timeout);
}

JSValue engine_register_audio_buffers (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    const auto it = engineInstances.find (magic);

    if (it == engineInstances.end ()) {
	return JS_ThrowReferenceError (ctx, "Could not find engine instance '%d' for registerAudioBuffers", magic);
    }

    int32_t resolution = 64;

    if (argc > 0) {
	JS_ToInt32 (ctx, &resolution, argv[0]);
    }

    return it->second.registerAudioBuffers (resolution);
}

EngineObject::EngineObject (ScriptEngine& engine, Render::Wallpapers::CScene& scene) :
    m_scene (scene), m_engine (engine), m_instanceId (++EngineInstanceId), m_classId (0) {
    engineInstances.emplace (this->m_instanceId, *this);

    this->m_definition = { .class_name = "IEngine" };
    JS_NewClassID (this->m_engine.getRuntime (), &this->m_classId);
    JS_NewClass (this->m_engine.getRuntime (), this->m_classId, &this->m_definition);
    this->m_instance = JS_NewObjectClass (this->m_engine.getContext (), this->m_classId);

    JS_DupValue (this->m_engine.getContext (), this->m_instance);

    // set properties
    JS_SetOpaque (this->m_instance, this);
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "frametime"),
	JS_NewCFunction (this->m_engine.getContext (), engine_get_frametime, "get", 0),
	JS_NewCFunction (this->m_engine.getContext (), engine_set_value, "set", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "runtime"),
	JS_NewCFunction (this->m_engine.getContext (), engine_get_runtime, "get", 0),
	JS_NewCFunction (this->m_engine.getContext (), engine_set_value, "set", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "timeOfDay"),
	JS_NewCFunction (this->m_engine.getContext (), engine_get_daytime, "get", 0),
	JS_NewCFunction (this->m_engine.getContext (), engine_set_value, "set", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "screenResolution"),
	JS_NewCFunction (this->m_engine.getContext (), engine_get_screen_resolution, "get", 0),
	JS_NewCFunction (this->m_engine.getContext (), engine_set_value, "set", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "canvasSize"),
	JS_NewCFunction (this->m_engine.getContext (), engine_get_canvas_size, "get", 0),
	JS_NewCFunction (this->m_engine.getContext (), engine_set_value, "set", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "AUDIO_RESOLUTION_16",
	JS_NewInt32 (this->m_engine.getContext (), 16), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "AUDIO_RESOLUTION_32",
	JS_NewInt32 (this->m_engine.getContext (), 32), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "AUDIO_RESOLUTION_64",
	JS_NewInt32 (this->m_engine.getContext (), 64), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "setInterval",
	JS_NewCFunctionMagic (
	    this->m_engine.getContext (), engine_set_interval, "setInterval", 2, JS_CFUNC_generic_magic,
	    this->m_instanceId
	),
	JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "clearInterval",
	JS_NewCFunctionMagic (
	    this->m_engine.getContext (), engine_clear_interval, "clearInterval", 1, JS_CFUNC_generic_magic,
	    this->m_instanceId
	),
	JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "setTimeout",
	JS_NewCFunctionMagic (
	    this->m_engine.getContext (), engine_set_timeout, "setTimeout", 2, JS_CFUNC_generic_magic,
	    this->m_instanceId
	),
	JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "clearTimeout",
	JS_NewCFunctionMagic (
	    this->m_engine.getContext (), engine_clear_timeout, "clearTimeout", 1, JS_CFUNC_generic_magic,
	    this->m_instanceId
	),
	JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "openUserShortcut",
	JS_NewCFunction (this->m_engine.getContext (), engine_open_user_shortcut, "openUserShortcut", 0),
	JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "isScreensaver",
	JS_NewCFunction (this->m_engine.getContext (), engine_is_screensaver, "isScreensaver", 0),
	JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "isRunningInEditor",
	JS_NewCFunction (this->m_engine.getContext (), engine_is_running_in_editor, "isRunningInEditor", 0),
	JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "isWallpaper",
	JS_NewCFunction (this->m_engine.getContext (), engine_is_wallpaper, "isWallpaper", 0),
	JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "isPortrait",
	JS_NewCFunctionMagic (
	    this->m_engine.getContext (), engine_is_portrait, "isPortrait", 0, JS_CFUNC_generic_magic,
	    this->m_instanceId
	),
	JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "registerAudioBuffers",
	JS_NewCFunctionMagic (
	    this->m_engine.getContext (), engine_register_audio_buffers, "registerAudioBuffers", 1,
	    JS_CFUNC_generic_magic, this->m_instanceId
	),
	JS_PROP_ENUMERABLE
    );
}

EngineObject::~EngineObject () {
    // clear all the timeouts and intervals
    for (const auto& [id, timeout] : this->m_timeouts) {
	JS_FreeValue (this->m_engine.getContext (), timeout.callback);
    }
    for (const auto& [id, interval] : this->m_intervals) {
	JS_FreeValue (this->m_engine.getContext (), interval.callback);
    }
    for (const auto& binding : this->m_audioBuffers) {
	JS_FreeValue (this->m_engine.getContext (), binding.average);
    }

    engineInstances.erase (this->m_instanceId);
    this->m_intervals.clear ();
    this->m_timeouts.clear ();
    this->m_audioBuffers.clear ();

    JS_FreeValue (this->m_engine.getContext (), this->m_instance);
}

uint32_t EngineObject::reserveNextTimeoutId (JSValue function, uint64_t duration) {
    const auto id = ++this->m_nextTimeoutId;

    // argv-sourced JSValues (e.g. from the setTimeout/setInterval JS bindings) are borrowed
    // references owned by the caller's stack frame -- QuickJS frees them once the enclosing
    // JS statement finishes. Storing one here beyond that point without duplicating it first
    // leaves this map holding a dangling reference that later use (JS_Call/JS_FreeValue in
    // tick()/clearTimeout/~EngineObject) touches as a use-after-free.
    this->m_timeouts[id] = Timeout { .callback = JS_DupValue (this->m_engine.getContext (), function),
				     .duration = std::chrono::milliseconds (duration),
				     .next = std::chrono::steady_clock::now () + std::chrono::milliseconds (duration) };

    return id;
}

uint32_t EngineObject::reserveNextIntervalId (JSValue function, uint64_t duration) {
    const auto id = ++this->m_nextIntervalId;

    // see the matching comment in reserveNextTimeoutId -- same borrowed-reference issue
    this->m_intervals[id]
	= Timeout { .callback = JS_DupValue (this->m_engine.getContext (), function),
		    .duration = std::chrono::milliseconds (duration),
		    .next = std::chrono::steady_clock::now () + std::chrono::milliseconds (duration) };

    return id;
}

void EngineObject::clearInterval (uint32_t id) {
    const auto it = this->m_intervals.find (id);

    if (it == this->m_intervals.end ()) {
	return;
    }

    JS_FreeValue (this->getEngine ().getContext (), it->second.callback);

    this->m_intervals.erase (id);
}

void EngineObject::clearTimeout (uint32_t id) {
    const auto it = this->m_timeouts.find (id);

    if (it == this->m_timeouts.end ()) {
	return;
    }

    JS_FreeValue (this->getEngine ().getContext (), it->second.callback);

    this->m_timeouts.erase (id);
}

JSValue EngineObject::registerAudioBuffers (int32_t resolution) {
    // the recorder only ever produces 16/32/64-band data (the same tiers backing
    // g_AudioSpectrum16/32/64); snap whatever was requested to the nearest one
    const uint32_t bands = resolution >= 64 ? 64 : (resolution >= 32 ? 32 : 16);
    JSContext* ctx = this->m_engine.getContext ();

    JSValue averageArray = JS_NewArray (ctx);

    for (uint32_t i = 0; i < bands; i++) {
	JS_SetPropertyUint32 (ctx, averageArray, i, JS_NewFloat64 (ctx, 0.0));
    }

    JSValue result = JS_NewObject (ctx);
    JS_SetPropertyStr (ctx, result, "average", JS_DupValue (ctx, averageArray));

    this->m_audioBuffers.push_back (AudioBufferBinding { .resolution = bands, .average = averageArray });

    return result;
}

void EngineObject::tick () {
    const auto now = std::chrono::steady_clock::now ();

    // refresh any registered audio buffers before running scripts, so update()
    // functions that read audioBuffer.average see this frame's data
    if (!this->m_audioBuffers.empty ()) {
	const auto& recorder = this->m_scene.getAudioContext ().getRecorder ();
	JSContext* ctx = this->m_engine.getContext ();

	for (const auto& binding : this->m_audioBuffers) {
	    const float* source = binding.resolution == 64 ? recorder.audio64
				 : binding.resolution == 32 ? recorder.audio32
							     : recorder.audio16;

	    for (uint32_t i = 0; i < binding.resolution; i++) {
		JS_SetPropertyUint32 (ctx, binding.average, i, JS_NewFloat64 (ctx, source[i]));
	    }
	}
    }

    JSContext* ctx = this->m_engine.getContext ();

    // Snapshot which intervals/timeouts are due *before* calling into any JS, since a
    // callback can reentrantly call engine.setInterval/setTimeout/clearInterval/
    // clearTimeout on this same EngineObject (a very common self-rescheduling pattern),
    // mutating m_intervals/m_timeouts while we'd otherwise still be iterating them —
    // erasing the entry currently being visited (e.g. a timeout clearing itself) leaves
    // a dangling reference and crashes on the next access to it.
    std::vector<uint32_t> dueIntervals;
    for (const auto& [id, timeout] : this->m_intervals) {
	if (timeout.next <= now) {
	    dueIntervals.push_back (id);
	}
    }

    // check any interval and run them if needed
    for (auto id : dueIntervals) {
	const auto it = this->m_intervals.find (id);

	if (it == this->m_intervals.end ()) {
	    continue; // cleared reentrantly by an earlier callback this tick
	}

	it->second.next = now + it->second.duration;

	// keep the callback alive for the duration of the call: a reentrant
	// clearInterval(id) from within the callback itself would otherwise free the
	// JSValue we're still executing
	const JSValue callback = JS_DupValue (ctx, it->second.callback);
	JS_Call (ctx, callback, JS_NULL, 0, nullptr);
	JS_FreeValue (ctx, callback);
    }

    std::vector<uint32_t> dueTimeouts;
    for (const auto& [id, timeout] : this->m_timeouts) {
	if (timeout.next <= now) {
	    dueTimeouts.push_back (id);
	}
    }

    // check any timeout and run them if needed
    for (auto id : dueTimeouts) {
	const auto it = this->m_timeouts.find (id);

	if (it == this->m_timeouts.end ()) {
	    continue; // cleared reentrantly by an earlier callback this tick
	}

	const JSValue callback = JS_DupValue (ctx, it->second.callback);
	JS_Call (ctx, callback, JS_NULL, 0, nullptr);
	JS_FreeValue (ctx, callback);

	// the callback may have already cleared this exact timeout (self-clearing
	// pattern); re-lookup rather than reusing `it`, which erase() may have
	// invalidated, and only free/erase what's still actually there
	const auto current = this->m_timeouts.find (id);

	if (current != this->m_timeouts.end ()) {
	    JS_FreeValue (ctx, current->second.callback);
	    this->m_timeouts.erase (current);
	}
    }
}