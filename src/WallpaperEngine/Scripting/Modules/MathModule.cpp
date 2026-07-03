#include "MathModule.h"

#include "WallpaperEngine/Scripting/ScriptEngine.h"

using namespace WallpaperEngine::Scripting::Modules;

#define min_f(a, b, c) (fminf (a, fminf (b, c)))
#define max_f(a, b, c) (fmaxf (a, fmaxf (b, c)))

static uint32_t MathModuleInstanceId = 0;
std::map<uint32_t, MathModule&> mathModules;

JSValue wemath_smoothstep (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    if (argc != 3) {
	return JS_EXCEPTION;
    }

    if (!JS_IsNumber (argv[0]) || !JS_IsNumber (argv[1]) || !JS_IsNumber (argv[2])) {
	return JS_EXCEPTION;
    }

    double edge0 = 0.0f;
    double edge1 = 1.0f;
    double x = 0.0f;

    JS_ToFloat64 (ctx, &edge0, argv[0]);
    JS_ToFloat64 (ctx, &edge1, argv[1]);
    JS_ToFloat64 (ctx, &x, argv[2]);

    return JS_NewFloat64 (ctx, glm::smoothstep (edge0, edge1, x));
}

JSValue wemath_mix (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    if (argc != 3) {
	return JS_EXCEPTION;
    }

    if (!JS_IsNumber (argv[0]) || !JS_IsNumber (argv[1]) || !JS_IsNumber (argv[2])) {
	return JS_EXCEPTION;
    }

    double a = 0.0f;
    double b = 1.0f;
    double value = 0.0f;

    JS_ToFloat64 (ctx, &a, argv[0]);
    JS_ToFloat64 (ctx, &b, argv[1]);
    JS_ToFloat64 (ctx, &value, argv[2]);

    return JS_NewFloat64 (ctx, glm::mix (a, b, value));
}

// Called lazily by QuickJS the first time this module is evaluated (not at JS_NewCModule time),
// so this is where the actual export VALUES must be bound — JS_SetModuleExport() only succeeds
// once the corresponding JS_AddModuleExport() name declaration exists, and those are registered
// eagerly in the constructor below, immediately after JS_NewCModule().
int wemath_init (JSContext* ctx, JSModuleDef* m) {
    for (const auto& [id, mod] : mathModules) {
	if (mod.getDefinition () != m) {
	    continue;
	}

	JS_SetModuleExport (
	    ctx, m, "smoothStep",
	    JS_NewCFunctionMagic (ctx, wemath_smoothstep, "smoothStep", 3, JS_CFUNC_generic_magic, id)
	);
	JS_SetModuleExport (
	    ctx, m, "mix", JS_NewCFunctionMagic (ctx, wemath_mix, "mix", 3, JS_CFUNC_generic_magic, id)
	);
	JS_SetModuleExport (ctx, m, "deg2rad", JS_NewFloat64 (ctx, 0.01745329251994329576923690768489));
	JS_SetModuleExport (ctx, m, "rad2deg", JS_NewFloat64 (ctx, 57.295779513082320876798154814105));

	break;
    }

    return 0;
}

MathModule::MathModule (ScriptEngine& engine) : ScriptModule (engine, "WEMath", wemath_init) {
    this->m_instanceId = ++MathModuleInstanceId;

    mathModules.emplace (this->m_instanceId, *this);

    // Declare the export names now, eagerly — QuickJS needs these known during module linking,
    // well before wemath_init() is invoked lazily at evaluation time to bind the actual values.
    JS_AddModuleExport (this->getEngine ().getContext (), this->getDefinition (), "smoothStep");
    JS_AddModuleExport (this->getEngine ().getContext (), this->getDefinition (), "mix");
    JS_AddModuleExport (this->getEngine ().getContext (), this->getDefinition (), "deg2rad");
    JS_AddModuleExport (this->getEngine ().getContext (), this->getDefinition (), "rad2deg");
}

MathModule::~MathModule () { mathModules.erase (this->m_instanceId); }
