#include "ConsoleObject.h"

#include "EngineObject.h"
#include "ScriptEngine.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"

using namespace WallpaperEngine::Scripting;

// Matches standard console.log/error formatting: arguments are space-separated, and
// objects/arrays print as JSON rather than the useless default "[object Object]" that
// JS_ToCString's implicit toString() conversion would otherwise produce.
static std::string console_format_args (JSContext* ctx, int argc, JSValueConst* argv) {
    std::stringstream stream;

    for (int i = 0; i < argc; i++) {
	if (i > 0) {
	    stream << ' ';
	}

	if (JS_IsObject (argv[i]) && !JS_IsFunction (ctx, argv[i])) {
	    JSValue json = JS_JSONStringify (ctx, argv[i], JS_UNDEFINED, JS_UNDEFINED);

	    if (JS_IsException (json)) {
		// e.g. a circular reference — clear the exception and fall back below rather
		// than letting it leak into whatever the script does next.
		JS_FreeValue (ctx, JS_GetException (ctx));
	    } else if (JS_IsString (json)) {
		const char* jsonStr = JS_ToCString (ctx, json);

		if (jsonStr) {
		    stream << jsonStr;
		    JS_FreeCString (ctx, jsonStr);
		    JS_FreeValue (ctx, json);
		    continue;
		}
	    }

	    JS_FreeValue (ctx, json);
	}

	const char* str = JS_ToCString (ctx, argv[i]);
	ScopeGuard guard ([ctx, str] { JS_FreeCString (ctx, str); });

	stream << (str ? str : "undefined");
    }

    return stream.str ();
}

JSValue console_log (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1) {
	return JS_UNDEFINED;
    }

    sLog.out (console_format_args (ctx, argc, argv));

    return JS_UNDEFINED;
}

JSValue console_error (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1) {
	return JS_UNDEFINED;
    }

    sLog.error (console_format_args (ctx, argc, argv));

    return JS_UNDEFINED;
}

ConsoleObject::ConsoleObject (ScriptEngine& engine, Render::Wallpapers::CScene& scene) :
    m_scene (scene), m_engine (engine), m_classId (0) {
    this->m_definition = { .class_name = "IConsole" };
    JS_NewClassID (this->m_engine.getRuntime (), &this->m_classId);
    JS_NewClass (this->m_engine.getRuntime (), this->m_classId, &this->m_definition);
    this->m_instance = JS_NewObjectClass (this->m_engine.getContext (), this->m_classId);

    JS_DupValue (this->m_engine.getContext (), this->m_instance);

    // set properties
    JS_SetOpaque (this->m_instance, this);
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "log",
	JS_NewCFunction (this->m_engine.getContext (), console_log, "log", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "error",
	JS_NewCFunction (this->m_engine.getContext (), console_error, "error", 1), JS_PROP_ENUMERABLE
    );
}

ConsoleObject::~ConsoleObject () { JS_FreeValue (this->m_engine.getContext (), this->m_instance); }