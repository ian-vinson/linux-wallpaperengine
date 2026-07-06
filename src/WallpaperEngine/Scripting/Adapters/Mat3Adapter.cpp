#include "Mat3Adapter.h"

#include "Mat4Adapter.h"
#include "VectorAdapter.h"
#include "WallpaperEngine/Scripting/ScriptEngine.h"

#include <cmath>
#include <glm/gtc/type_ptr.hpp>
#include <map>
#include <string>

using namespace WallpaperEngine::Scripting;
using namespace WallpaperEngine::Scripting::Adapters;

namespace {
constexpr float kMat3Epsilon = 1e-5f;

// magic value used to ensure the opaque value we got back is actually a Mat3 container
constexpr unsigned int kMat3OpaqueMagic = 0x4d613300u; // "Ma3\0"

struct Mat3OpaqueContainer {
    unsigned int magic;
    Mat3Adapter& adapter;
    glm::mat3 value;
};

#define MAT3_MAGIC_CHECK_EXCEPTION(container)                                                                        \
    do {                                                                                                             \
        if (!(container) || (container)->magic != kMat3OpaqueMagic) {                                               \
            return JS_ThrowTypeError (ctx, "not a Mat3 object");                                                     \
        }                                                                                                            \
    } while (0)

std::map<uint32_t, Mat3Adapter&> g_mat3AdapterInstances;
uint32_t g_mat3AdapterInstanceId = 0;

Mat3OpaqueContainer* getContainer (JSValueConst value) {
    JSClassID classId = 0;
    auto* container = static_cast<Mat3OpaqueContainer*> (JS_GetAnyOpaque (value, &classId));

    return (container != nullptr && container->magic == kMat3OpaqueMagic) ? container : nullptr;
}

JSValue makeVec2 (ScriptEngine& engine, const glm::vec2& v) {
    JSContext* ctx = engine.getContext ();
    JSValue vec = engine.getAdapters ().vec2->instantiate ();

    JS_SetPropertyStr (ctx, vec, "x", JS_NewFloat64 (ctx, v.x));
    JS_SetPropertyStr (ctx, vec, "y", JS_NewFloat64 (ctx, v.y));

    return vec;
}

JSValue makeVec3 (ScriptEngine& engine, const glm::vec3& v) {
    JSContext* ctx = engine.getContext ();
    JSValue vec = engine.getAdapters ().vec3->instantiate ();

    JS_SetPropertyStr (ctx, vec, "x", JS_NewFloat64 (ctx, v.x));
    JS_SetPropertyStr (ctx, vec, "y", JS_NewFloat64 (ctx, v.y));
    JS_SetPropertyStr (ctx, vec, "z", JS_NewFloat64 (ctx, v.z));

    return vec;
}

bool tryGetVec2 (JSContext* ctx, JSValueConst v, glm::vec2& out) {
    if (!JS_IsObject (v)) {
        return false;
    }

    JSValue x = JS_GetPropertyStr (ctx, v, "x");
    JSValue y = JS_GetPropertyStr (ctx, v, "y");
    bool ok = JS_IsNumber (x) && JS_IsNumber (y);

    if (ok) {
        double xv = 0.0, yv = 0.0;
        JS_ToFloat64 (ctx, &xv, x);
        JS_ToFloat64 (ctx, &yv, y);
        out = glm::vec2 (xv, yv);
    }

    JS_FreeValue (ctx, x);
    JS_FreeValue (ctx, y);

    return ok;
}

bool tryGetNumberOrVec2 (JSContext* ctx, JSValueConst v, glm::vec2& out) {
    if (JS_IsNumber (v)) {
        double n = 0.0;
        JS_ToFloat64 (ctx, &n, v);
        out = glm::vec2 (static_cast<float> (n));
        return true;
    }

    return tryGetVec2 (ctx, v, out);
}

// Duck-typed Vec3 detection for multiply()'s polymorphic argument (Mat3|Vec3|Number).
bool tryGetVec3 (JSContext* ctx, JSValueConst v, glm::vec3& out) {
    if (!JS_IsObject (v)) {
        return false;
    }

    JSValue x = JS_GetPropertyStr (ctx, v, "x");
    JSValue y = JS_GetPropertyStr (ctx, v, "y");
    JSValue z = JS_GetPropertyStr (ctx, v, "z");
    bool ok = JS_IsNumber (x) && JS_IsNumber (y) && JS_IsNumber (z);

    if (ok) {
        double xv = 0.0, yv = 0.0, zv = 0.0;
        JS_ToFloat64 (ctx, &xv, x);
        JS_ToFloat64 (ctx, &yv, y);
        JS_ToFloat64 (ctx, &zv, z);
        out = glm::vec3 (xv, yv, zv);
    }

    JS_FreeValue (ctx, x);
    JS_FreeValue (ctx, y);
    JS_FreeValue (ctx, z);

    return ok;
}

glm::mat3 makeRotation (float angleDegrees) {
    const float a = glm::radians (angleDegrees);
    glm::mat3 m (1.0f);
    m[0] = glm::vec3 (std::cos (a), std::sin (a), 0.0f);
    m[1] = glm::vec3 (-std::sin (a), std::cos (a), 0.0f);

    return m;
}

std::string formatComponents (const float* ptr, int count) {
    std::string result;

    for (int i = 0; i < count; i++) {
        if (i > 0) {
            result += ", ";
        }

        result += std::to_string (ptr[i]);
    }

    return result;
}

// ---------------------------------------------------------------------------------------------
// `m` property — ordinary getter/setter on the prototype (see Mat4Adapter.h for the rationale).
// ---------------------------------------------------------------------------------------------

JSValue mat3_get_m (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* container = getContainer (this_val);
    MAT3_MAGIC_CHECK_EXCEPTION (container);

    JSValue array = JS_NewArray (ctx);
    const float* ptr = glm::value_ptr (container->value);

    for (int i = 0; i < 9; i++) {
        JS_SetPropertyUint32 (ctx, array, i, JS_NewFloat64 (ctx, ptr[i]));
    }

    return array;
}

JSValue mat3_set_m (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT3_MAGIC_CHECK_EXCEPTION (container);

    if (argc < 1 || !JS_IsArray (argv[0])) {
        return JS_ThrowTypeError (ctx, "m must be assigned an Array of 9 numbers");
    }

    JSValue lengthValue = JS_GetPropertyStr (ctx, argv[0], "length");
    int32_t length = 0;
    JS_ToInt32 (ctx, &length, lengthValue);
    JS_FreeValue (ctx, lengthValue);

    if (length != 9) {
        return JS_ThrowTypeError (ctx, "m must have exactly 9 elements for a Mat3");
    }

    float* ptr = glm::value_ptr (container->value);

    for (int i = 0; i < 9; i++) {
        JSValue element = JS_GetPropertyUint32 (ctx, argv[0], i);
        double value = 0.0;
        JS_ToFloat64 (ctx, &value, element);
        JS_FreeValue (ctx, element);
        ptr[i] = static_cast<float> (value);
    }

    return JS_UNDEFINED;
}

// ---------------------------------------------------------------------------------------------
// Constructor + static factories
// ---------------------------------------------------------------------------------------------

JSValue mat3_constructor (JSContext* ctx, JSValueConst, int, JSValueConst*, int magic) {
    const auto it = g_mat3AdapterInstances.find (magic);

    if (it == g_mat3AdapterInstances.end ()) {
        return JS_ThrowInternalError (ctx, "Mat3 adapter is no longer available");
    }

    return it->second.wrap (glm::mat3 (1.0f));
}

JSValue mat3_static_identity (JSContext* ctx, JSValueConst, int, JSValueConst*, int magic) {
    const auto it = g_mat3AdapterInstances.find (magic);

    if (it == g_mat3AdapterInstances.end ()) {
        return JS_ThrowInternalError (ctx, "Mat3 adapter is no longer available");
    }

    return it->second.wrap (glm::mat3 (1.0f));
}

JSValue mat3_static_fromTranslation (JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic) {
    const auto it = g_mat3AdapterInstances.find (magic);

    if (it == g_mat3AdapterInstances.end ()) {
        return JS_ThrowInternalError (ctx, "Mat3 adapter is no longer available");
    }

    glm::vec2 v {};

    if (argc < 1 || !tryGetVec2 (ctx, argv[0], v)) {
        return JS_ThrowTypeError (ctx, "fromTranslation() requires a Vec2 argument");
    }

    glm::mat3 result (1.0f);
    result[2] = glm::vec3 (v, 1.0f);

    return it->second.wrap (result);
}

JSValue mat3_static_fromScale (JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic) {
    const auto it = g_mat3AdapterInstances.find (magic);

    if (it == g_mat3AdapterInstances.end ()) {
        return JS_ThrowInternalError (ctx, "Mat3 adapter is no longer available");
    }

    glm::vec2 v {};

    if (argc < 1 || !tryGetNumberOrVec2 (ctx, argv[0], v)) {
        return JS_ThrowTypeError (ctx, "fromScale() requires a Number or Vec2 argument");
    }

    glm::mat3 result (1.0f);
    result[0].x = v.x;
    result[1].y = v.y;

    return it->second.wrap (result);
}

JSValue mat3_static_fromRotation (JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic) {
    const auto it = g_mat3AdapterInstances.find (magic);

    if (it == g_mat3AdapterInstances.end ()) {
        return JS_ThrowInternalError (ctx, "Mat3 adapter is no longer available");
    }

    if (argc < 1 || !JS_IsNumber (argv[0])) {
        return JS_ThrowTypeError (ctx, "fromRotation() requires (angle: Number)");
    }

    double angle = 0.0;
    JS_ToFloat64 (ctx, &angle, argv[0]);

    return it->second.wrap (makeRotation (static_cast<float> (angle)));
}

JSValue mat3_static_fromBasis (JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic) {
    const auto it = g_mat3AdapterInstances.find (magic);

    if (it == g_mat3AdapterInstances.end ()) {
        return JS_ThrowInternalError (ctx, "Mat3 adapter is no longer available");
    }

    glm::vec2 right {}, up {};

    if (argc < 2 || !tryGetVec2 (ctx, argv[0], right) || !tryGetVec2 (ctx, argv[1], up)) {
        return JS_ThrowTypeError (ctx, "fromBasis() requires (right: Vec2, up: Vec2)");
    }

    glm::mat3 result (1.0f);
    result[0] = glm::vec3 (right, 0.0f);
    result[1] = glm::vec3 (up, 0.0f);

    return it->second.wrap (result);
}

JSValue mat3_static_fromMat4 (JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic) {
    const auto it = g_mat3AdapterInstances.find (magic);

    if (it == g_mat3AdapterInstances.end ()) {
        return JS_ThrowInternalError (ctx, "Mat3 adapter is no longer available");
    }

    glm::mat4 source {};

    if (argc < 1 || !it->second.getEngine ().getAdapters ().mat4->extract (argv[0], source)) {
        return JS_ThrowTypeError (ctx, "fromMat4() requires a Mat4 argument");
    }

    return it->second.wrap (glm::mat3 (source));
}

JSValue mat3_static_compose (JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic) {
    const auto it = g_mat3AdapterInstances.find (magic);

    if (it == g_mat3AdapterInstances.end ()) {
        return JS_ThrowInternalError (ctx, "Mat3 adapter is no longer available");
    }

    glm::vec2 translation {}, scale {};

    if (argc < 3 || !tryGetVec2 (ctx, argv[0], translation) || !JS_IsNumber (argv[1])
        || !tryGetVec2 (ctx, argv[2], scale)) {
        return JS_ThrowTypeError (ctx, "compose() requires (translation: Vec2, rotation: Number, scale: Vec2)");
    }

    double rotation = 0.0;
    JS_ToFloat64 (ctx, &rotation, argv[1]);

    glm::mat3 t (1.0f);
    t[2] = glm::vec3 (translation, 1.0f);
    const glm::mat3 r = makeRotation (static_cast<float> (rotation));
    glm::mat3 s (1.0f);
    s[0].x = scale.x;
    s[1].y = scale.y;

    return it->second.wrap (t * r * s);
}

// ---------------------------------------------------------------------------------------------
// Instance (prototype) methods
// ---------------------------------------------------------------------------------------------

JSValue mat3_translation (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT3_MAGIC_CHECK_EXCEPTION (container);

    if (argc >= 1 && !JS_IsUndefined (argv[0])) {
        glm::vec2 v {};

        if (!tryGetVec2 (ctx, argv[0], v)) {
            return JS_ThrowTypeError (ctx, "translation() requires a Vec2 argument");
        }

        container->value[2].x = v.x;
        container->value[2].y = v.y;
    }

    return makeVec2 (container->adapter.getEngine (), glm::vec2 (container->value[2]));
}

JSValue mat3_angle (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* container = getContainer (this_val);
    MAT3_MAGIC_CHECK_EXCEPTION (container);

    const glm::vec3& c0 = container->value[0];

    return JS_NewFloat64 (ctx, glm::degrees (std::atan2 (c0.y, c0.x)));
}

JSValue mat3_add (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT3_MAGIC_CHECK_EXCEPTION (container);

    auto* other = argc >= 1 ? getContainer (argv[0]) : nullptr;

    if (!other) {
        return JS_ThrowTypeError (ctx, "add() requires a Mat3 argument");
    }

    return container->adapter.wrap (container->value + other->value);
}

JSValue mat3_subtract (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT3_MAGIC_CHECK_EXCEPTION (container);

    auto* other = argc >= 1 ? getContainer (argv[0]) : nullptr;

    if (!other) {
        return JS_ThrowTypeError (ctx, "subtract() requires a Mat3 argument");
    }

    return container->adapter.wrap (container->value - other->value);
}

JSValue mat3_multiply (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT3_MAGIC_CHECK_EXCEPTION (container);

    if (argc < 1) {
        return JS_ThrowTypeError (ctx, "multiply() requires a Mat3, Vec3, or Number argument");
    }

    if (JS_IsNumber (argv[0])) {
        double scalar = 0.0;
        JS_ToFloat64 (ctx, &scalar, argv[0]);

        return container->adapter.wrap (container->value * static_cast<float> (scalar));
    }

    if (auto* other = getContainer (argv[0])) {
        return container->adapter.wrap (container->value * other->value);
    }

    glm::vec3 v {};

    if (tryGetVec3 (ctx, argv[0], v)) {
        return makeVec3 (container->adapter.getEngine (), container->value * v);
    }

    return JS_ThrowTypeError (ctx, "multiply() requires a Mat3, Vec3, or Number argument");
}

JSValue mat3_translate (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT3_MAGIC_CHECK_EXCEPTION (container);

    glm::vec2 v {};

    if (argc < 1 || !tryGetVec2 (ctx, argv[0], v)) {
        return JS_ThrowTypeError (ctx, "translate() requires a Vec2 argument");
    }

    glm::mat3 t (1.0f);
    t[2] = glm::vec3 (v, 1.0f);

    return container->adapter.wrap (container->value * t);
}

JSValue mat3_rotate (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT3_MAGIC_CHECK_EXCEPTION (container);

    if (argc < 1 || !JS_IsNumber (argv[0])) {
        return JS_ThrowTypeError (ctx, "rotate() requires (angle: Number)");
    }

    double angle = 0.0;
    JS_ToFloat64 (ctx, &angle, argv[0]);

    return container->adapter.wrap (container->value * makeRotation (static_cast<float> (angle)));
}

JSValue mat3_scale (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT3_MAGIC_CHECK_EXCEPTION (container);

    glm::vec2 v {};

    if (argc < 1 || !tryGetNumberOrVec2 (ctx, argv[0], v)) {
        return JS_ThrowTypeError (ctx, "scale() requires a Number or Vec2 argument");
    }

    glm::mat3 s (1.0f);
    s[0].x = v.x;
    s[1].y = v.y;

    return container->adapter.wrap (container->value * s);
}

JSValue mat3_transformPoint (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT3_MAGIC_CHECK_EXCEPTION (container);

    glm::vec2 v {};

    if (argc < 1 || !tryGetVec2 (ctx, argv[0], v)) {
        return JS_ThrowTypeError (ctx, "transformPoint() requires a Vec2 argument");
    }

    const glm::vec3 result = container->value * glm::vec3 (v, 1.0f);

    return makeVec2 (container->adapter.getEngine (), glm::vec2 (result));
}

JSValue mat3_transformDirection (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT3_MAGIC_CHECK_EXCEPTION (container);

    glm::vec2 v {};

    if (argc < 1 || !tryGetVec2 (ctx, argv[0], v)) {
        return JS_ThrowTypeError (ctx, "transformDirection() requires a Vec2 argument");
    }

    const glm::vec3 result = container->value * glm::vec3 (v, 0.0f);

    return makeVec2 (container->adapter.getEngine (), glm::vec2 (result));
}

JSValue mat3_transpose (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* container = getContainer (this_val);
    MAT3_MAGIC_CHECK_EXCEPTION (container);

    return container->adapter.wrap (glm::transpose (container->value));
}

JSValue mat3_determinant (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* container = getContainer (this_val);
    MAT3_MAGIC_CHECK_EXCEPTION (container);

    return JS_NewFloat64 (ctx, glm::determinant (container->value));
}

JSValue mat3_inverse (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* container = getContainer (this_val);
    MAT3_MAGIC_CHECK_EXCEPTION (container);

    return container->adapter.wrap (glm::inverse (container->value));
}

JSValue mat3_decompose (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* container = getContainer (this_val);
    MAT3_MAGIC_CHECK_EXCEPTION (container);

    const glm::vec2 translation (container->value[2]);
    const glm::vec3& c0 = container->value[0];
    const glm::vec3& c1 = container->value[1];
    const glm::vec2 scale (glm::length (glm::vec2 (c0)), glm::length (glm::vec2 (c1)));
    const double rotation = glm::degrees (std::atan2 (c0.y, c0.x));

    JSValue result = JS_NewObject (ctx);
    JS_SetPropertyStr (ctx, result, "translation", makeVec2 (container->adapter.getEngine (), translation));
    JS_SetPropertyStr (ctx, result, "rotation", JS_NewFloat64 (ctx, rotation));
    JS_SetPropertyStr (ctx, result, "scale", makeVec2 (container->adapter.getEngine (), scale));

    return result;
}

JSValue mat3_copy (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* container = getContainer (this_val);
    MAT3_MAGIC_CHECK_EXCEPTION (container);

    return container->adapter.wrap (container->value);
}

JSValue mat3_equals (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT3_MAGIC_CHECK_EXCEPTION (container);

    auto* other = argc >= 1 ? getContainer (argv[0]) : nullptr;

    if (!other) {
        return JS_ThrowTypeError (ctx, "equals() requires a Mat3 argument");
    }

    const float* a = glm::value_ptr (container->value);
    const float* b = glm::value_ptr (other->value);
    bool equal = true;

    for (int i = 0; i < 9 && equal; i++) {
        equal = std::fabs (a[i] - b[i]) <= kMat3Epsilon;
    }

    return JS_NewBool (ctx, equal);
}

JSValue mat3_toString (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* container = getContainer (this_val);
    MAT3_MAGIC_CHECK_EXCEPTION (container);

    return JS_NewString (ctx, formatComponents (glm::value_ptr (container->value), 9).c_str ());
}

} // namespace

Mat3Adapter::Mat3Adapter (ScriptEngine& engine) :
    ObjectAdapter (engine), m_name ("Mat3"), m_instanceId (++g_mat3AdapterInstanceId) {
    g_mat3AdapterInstances.emplace (this->m_instanceId, *this);

    this->registerType ({
        .class_name = this->m_name.c_str (),
        .finalizer = [] (JSRuntime*, JSValueConst val) {
            JSClassID classId = 0;
            auto* container = static_cast<Mat3OpaqueContainer*> (JS_GetAnyOpaque (val, &classId));

            if (container != nullptr && container->magic == kMat3OpaqueMagic) {
                delete container;
            }
        },
    });

    m_prototype = JS_NewObject (this->m_engine.getContext ());
    JS_DupValue (this->m_engine.getContext (), m_prototype);

    JSValue ctor = JS_NewCFunctionMagic (
        this->m_engine.getContext (), mat3_constructor, this->m_name.c_str (), 0, JS_CFUNC_constructor_magic,
        this->m_instanceId
    );

    JS_SetConstructor (this->m_engine.getContext (), ctor, m_prototype);

    JS_DefinePropertyGetSet (
        this->m_engine.getContext (), m_prototype, JS_NewAtom (this->m_engine.getContext (), "m"),
        JS_NewCFunction (this->m_engine.getContext (), mat3_get_m, "get", 0),
        JS_NewCFunction (this->m_engine.getContext (), mat3_set_m, "set", 1), JS_PROP_ENUMERABLE
    );

    const struct {
        const char* name;
        JSCFunction* func;
        int length;
    } methods[] = {
        { "translation", mat3_translation, 0 },
        { "angle", mat3_angle, 0 },
        { "add", mat3_add, 1 },
        { "subtract", mat3_subtract, 1 },
        { "multiply", mat3_multiply, 1 },
        { "translate", mat3_translate, 1 },
        { "rotate", mat3_rotate, 1 },
        { "scale", mat3_scale, 1 },
        { "transformPoint", mat3_transformPoint, 1 },
        { "transformDirection", mat3_transformDirection, 1 },
        { "transpose", mat3_transpose, 0 },
        { "determinant", mat3_determinant, 0 },
        { "inverse", mat3_inverse, 0 },
        { "decompose", mat3_decompose, 0 },
        { "copy", mat3_copy, 0 },
        { "equals", mat3_equals, 1 },
        { "toString", mat3_toString, 0 },
    };

    for (const auto& method : methods) {
        JS_DefinePropertyValueStr (
            this->m_engine.getContext (), m_prototype, method.name,
            JS_NewCFunction (this->m_engine.getContext (), method.func, method.name, method.length), JS_PROP_ENUMERABLE
        );
    }

    const struct {
        const char* name;
        JSCFunctionMagic* func;
        int length;
    } statics[] = {
        { "identity", mat3_static_identity, 0 },
        { "fromTranslation", mat3_static_fromTranslation, 1 },
        { "fromScale", mat3_static_fromScale, 1 },
        { "fromRotation", mat3_static_fromRotation, 1 },
        { "fromBasis", mat3_static_fromBasis, 2 },
        { "fromMat4", mat3_static_fromMat4, 1 },
        { "compose", mat3_static_compose, 3 },
    };

    for (const auto& method : statics) {
        JS_DefinePropertyValueStr (
            this->m_engine.getContext (), ctor, method.name,
            JS_NewCFunctionMagic (
                this->m_engine.getContext (), method.func, method.name, method.length, JS_CFUNC_generic_magic,
                this->m_instanceId
            ),
            JS_PROP_ENUMERABLE
        );
    }

    JS_SetClassProto (this->m_engine.getContext (), this->m_classId, m_prototype);

    JS_SetPropertyStr (this->m_engine.getContext (), this->m_engine.getGlobalThis (), this->m_name.c_str (), ctor);
}

Mat3Adapter::~Mat3Adapter () { g_mat3AdapterInstances.erase (this->m_instanceId); }

void Mat3Adapter::releaseBeforeRuntimeShutdown () {
    JS_FreeValue (this->m_engine.getContext (), m_prototype);
    m_prototype = JS_UNDEFINED;
}

JSValue Mat3Adapter::wrap (const glm::mat3& value) {
    JSValue result = JS_NewObjectClass (this->m_engine.getContext (), this->m_classId);
    JS_SetOpaque (result, new Mat3OpaqueContainer { kMat3OpaqueMagic, *this, value });

    return result;
}

bool Mat3Adapter::extract (JSValueConst value, glm::mat3& out) const {
    auto* container = getContainer (value);

    if (container == nullptr) {
        return false;
    }

    out = container->value;

    return true;
}
