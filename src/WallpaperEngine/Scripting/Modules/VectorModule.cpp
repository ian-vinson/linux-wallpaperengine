#include "VectorModule.h"

#include "WallpaperEngine/Scripting/ScriptEngine.h"

#include <cmath>

using namespace WallpaperEngine::Scripting::Modules;

static uint32_t VectorModuleInstanceId = 0;
std::map<uint32_t, VectorModule&> vectorModules;

JSValue wevector_anglevector2 (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    if (argc != 1 || !JS_IsNumber (argv[0])) {
	return JS_ThrowTypeError (ctx, "angleVector2() expects 1 numeric argument");
    }

    double angle = 0.0;

    JS_ToFloat64 (ctx, &angle, argv[0]);

    const double rad = angle * (M_PI / 180.0);

    const auto it = vectorModules.find (magic);

    if (it == vectorModules.end ()) {
	return JS_UNDEFINED;
    }

    JSValue value = it->second.getEngine ().getAdapters ().vec2->instantiate ();

    JS_SetPropertyStr (ctx, value, "x", JS_NewFloat64 (ctx, std::cos (rad)));
    JS_SetPropertyStr (ctx, value, "y", JS_NewFloat64 (ctx, std::sin (rad)));

    return value;
}

JSValue wevector_vectorangle2 (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    if (argc != 1) {
	return JS_ThrowTypeError (ctx, "vectorAngle2() expects 1 argument");
    }

    if (JS_VALUE_GET_TAG (argv[0]) != JS_TAG_OBJECT) {
	return JS_ThrowTypeError (ctx, "vectorAngle2() expects an object with x and y properties");
    }

    JSValue x = JS_GetPropertyStr (ctx, argv[0], "x");
    JSValue y = JS_GetPropertyStr (ctx, argv[0], "y");

    if (!JS_IsNumber (x) || !JS_IsNumber (y)) {
	return JS_ThrowTypeError (ctx, "vectorAngle2() argument must have numeric x and y properties");
    }

    double xVal = 0.0, yVal = 0.0;

    JS_ToFloat64 (ctx, &xVal, x);
    JS_ToFloat64 (ctx, &yVal, y);

    return JS_NewFloat64 (ctx, std::atan2 (yVal, xVal) * (180.0 / M_PI));
}

// Called lazily by QuickJS the first time this module is evaluated (not at JS_NewCModule time),
// so this is where the actual export VALUES must be bound — JS_SetModuleExport() only succeeds
// once the corresponding JS_AddModuleExport() name declaration exists, and those are registered
// eagerly in the constructor below, immediately after JS_NewCModule().
int wevector_init (JSContext* ctx, JSModuleDef* m) {
    for (const auto& [id, mod] : vectorModules) {
	if (mod.getDefinition () != m) {
	    continue;
	}

	JS_SetModuleExport (
	    ctx, m, "angleVector2",
	    JS_NewCFunctionMagic (ctx, wevector_anglevector2, "angleVector2", 1, JS_CFUNC_generic_magic, id)
	);
	JS_SetModuleExport (
	    ctx, m, "vectorAngle2",
	    JS_NewCFunctionMagic (ctx, wevector_vectorangle2, "vectorAngle2", 1, JS_CFUNC_generic_magic, id)
	);

	break;
    }

    return 0;
}

VectorModule::VectorModule (ScriptEngine& engine) : ScriptModule (engine, "WEVector", wevector_init) {
    this->m_instanceId = ++VectorModuleInstanceId;

    vectorModules.emplace (this->m_instanceId, *this);

    // Declare the export names now, eagerly — QuickJS needs these known during module linking,
    // well before wevector_init() is invoked lazily at evaluation time to bind the actual values.
    JS_AddModuleExport (this->getEngine ().getContext (), this->getDefinition (), "angleVector2");
    JS_AddModuleExport (this->getEngine ().getContext (), this->getDefinition (), "vectorAngle2");
}

VectorModule::~VectorModule () { vectorModules.erase (this->m_instanceId); }
