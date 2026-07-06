#include "Mat4Adapter.h"

#include "Mat3Adapter.h"
#include "VectorAdapter.h"
#include "WallpaperEngine/Scripting/ScriptEngine.h"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#undef GLM_ENABLE_EXPERIMENTAL
#include <map>
#include <string>

using namespace WallpaperEngine::Scripting;
using namespace WallpaperEngine::Scripting::Adapters;

namespace {
constexpr float kMat4Epsilon = 1e-5f;

// magic value used to ensure the opaque value we got back is actually a Mat4 container
constexpr unsigned int kMat4OpaqueMagic = 0x4d613400u; // "Ma4\0"

struct Mat4OpaqueContainer {
    unsigned int magic;
    Mat4Adapter& adapter;
    glm::mat4 value;
};

#define MAT4_MAGIC_CHECK_EXCEPTION(container)                                                                        \
    do {                                                                                                             \
        if (!(container) || (container)->magic != kMat4OpaqueMagic) {                                               \
            return JS_ThrowTypeError (ctx, "not a Mat4 object");                                                     \
        }                                                                                                            \
    } while (0)

std::map<uint32_t, Mat4Adapter&> g_mat4AdapterInstances;
uint32_t g_mat4AdapterInstanceId = 0;

Mat4OpaqueContainer* getContainer (JSValueConst value) {
    JSClassID classId = 0;
    auto* container = static_cast<Mat4OpaqueContainer*> (JS_GetAnyOpaque (value, &classId));

    return (container != nullptr && container->magic == kMat4OpaqueMagic) ? container : nullptr;
}

JSValue makeVec3 (ScriptEngine& engine, const glm::vec3& v) {
    JSContext* ctx = engine.getContext ();
    JSValue vec = engine.getAdapters ().vec3->instantiate ();

    JS_SetPropertyStr (ctx, vec, "x", JS_NewFloat64 (ctx, v.x));
    JS_SetPropertyStr (ctx, vec, "y", JS_NewFloat64 (ctx, v.y));
    JS_SetPropertyStr (ctx, vec, "z", JS_NewFloat64 (ctx, v.z));

    return vec;
}

JSValue makeVec4 (ScriptEngine& engine, const glm::vec4& v) {
    JSContext* ctx = engine.getContext ();
    JSValue vec = engine.getAdapters ().vec4->instantiate ();

    JS_SetPropertyStr (ctx, vec, "x", JS_NewFloat64 (ctx, v.x));
    JS_SetPropertyStr (ctx, vec, "y", JS_NewFloat64 (ctx, v.y));
    JS_SetPropertyStr (ctx, vec, "z", JS_NewFloat64 (ctx, v.z));
    JS_SetPropertyStr (ctx, vec, "w", JS_NewFloat64 (ctx, v.w));

    return vec;
}

// Duck-typed extraction, mirroring VectorAdapter's own vector_get(JSContext*, JSValue) approach
// (component objects are matched by property shape, not by class identity).
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

// Accepts a Vec2 (z defaults to 0) or a Vec3 — used for translation-shaped (Vec2|Vec3) arguments.
bool tryGetVec2or3 (JSContext* ctx, JSValueConst v, glm::vec3& out) {
    if (!JS_IsObject (v)) {
        return false;
    }

    JSValue x = JS_GetPropertyStr (ctx, v, "x");
    JSValue y = JS_GetPropertyStr (ctx, v, "y");
    JSValue z = JS_GetPropertyStr (ctx, v, "z");
    bool ok = JS_IsNumber (x) && JS_IsNumber (y);

    if (ok) {
        double xv = 0.0, yv = 0.0, zv = 0.0;
        JS_ToFloat64 (ctx, &xv, x);
        JS_ToFloat64 (ctx, &yv, y);

        if (JS_IsNumber (z)) {
            JS_ToFloat64 (ctx, &zv, z);
        }

        out = glm::vec3 (xv, yv, zv);
    }

    JS_FreeValue (ctx, x);
    JS_FreeValue (ctx, y);
    JS_FreeValue (ctx, z);

    return ok;
}

// Accepts a Number (uniform scale) or a Vec3 — used for scale-shaped (Number|Vec3) arguments.
bool tryGetNumberOrVec3 (JSContext* ctx, JSValueConst v, glm::vec3& out) {
    if (JS_IsNumber (v)) {
        double n = 0.0;
        JS_ToFloat64 (ctx, &n, v);
        out = glm::vec3 (static_cast<float> (n));
        return true;
    }

    return tryGetVec3 (ctx, v, out);
}

// Duck-typed Vec4 detection for multiply()'s polymorphic argument (Mat4|Vec4|Number).
bool tryGetVec4 (JSContext* ctx, JSValueConst v, glm::vec4& out) {
    if (!JS_IsObject (v)) {
        return false;
    }

    JSValue x = JS_GetPropertyStr (ctx, v, "x");
    JSValue y = JS_GetPropertyStr (ctx, v, "y");
    JSValue z = JS_GetPropertyStr (ctx, v, "z");
    JSValue w = JS_GetPropertyStr (ctx, v, "w");
    bool ok = JS_IsNumber (x) && JS_IsNumber (y) && JS_IsNumber (z) && JS_IsNumber (w);

    if (ok) {
        double xv = 0.0, yv = 0.0, zv = 0.0, wv = 0.0;
        JS_ToFloat64 (ctx, &xv, x);
        JS_ToFloat64 (ctx, &yv, y);
        JS_ToFloat64 (ctx, &zv, z);
        JS_ToFloat64 (ctx, &wv, w);
        out = glm::vec4 (xv, yv, zv, wv);
    }

    JS_FreeValue (ctx, x);
    JS_FreeValue (ctx, y);
    JS_FreeValue (ctx, z);
    JS_FreeValue (ctx, w);

    return ok;
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
// `m` property — ordinary getter/setter on the prototype (see Mat4Adapter.h for why this isn't
// a class-level exotic property like VectorAdapter's x/y/z/w).
// ---------------------------------------------------------------------------------------------

JSValue mat4_get_m (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    JSValue array = JS_NewArray (ctx);
    const float* ptr = glm::value_ptr (container->value);

    for (int i = 0; i < 16; i++) {
        JS_SetPropertyUint32 (ctx, array, i, JS_NewFloat64 (ctx, ptr[i]));
    }

    return array;
}

JSValue mat4_set_m (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    if (argc < 1 || !JS_IsArray (argv[0])) {
        return JS_ThrowTypeError (ctx, "m must be assigned an Array of 16 numbers");
    }

    JSValue lengthValue = JS_GetPropertyStr (ctx, argv[0], "length");
    int32_t length = 0;
    JS_ToInt32 (ctx, &length, lengthValue);
    JS_FreeValue (ctx, lengthValue);

    if (length != 16) {
        return JS_ThrowTypeError (ctx, "m must have exactly 16 elements for a Mat4");
    }

    float* ptr = glm::value_ptr (container->value);

    for (int i = 0; i < 16; i++) {
        JSValue element = JS_GetPropertyUint32 (ctx, argv[0], i);
        double value = 0.0;
        JS_ToFloat64 (ctx, &value, element);
        JS_FreeValue (ctx, element);
        ptr[i] = static_cast<float> (value);
    }

    return JS_UNDEFINED;
}

// ---------------------------------------------------------------------------------------------
// Constructor + static factories — magic-based adapter-instance lookup, same mechanism as
// VectorAdapter's vector_constructor<>/vector3_fromSpherical (there's no `this` instance yet to
// read an adapter reference from).
// ---------------------------------------------------------------------------------------------

JSValue mat4_constructor (JSContext* ctx, JSValueConst, int, JSValueConst*, int magic) {
    const auto it = g_mat4AdapterInstances.find (magic);

    if (it == g_mat4AdapterInstances.end ()) {
        return JS_ThrowInternalError (ctx, "Mat4 adapter is no longer available");
    }

    return it->second.wrap (glm::mat4 (1.0f));
}

JSValue mat4_static_identity (JSContext* ctx, JSValueConst, int, JSValueConst*, int magic) {
    const auto it = g_mat4AdapterInstances.find (magic);

    if (it == g_mat4AdapterInstances.end ()) {
        return JS_ThrowInternalError (ctx, "Mat4 adapter is no longer available");
    }

    return it->second.wrap (glm::mat4 (1.0f));
}

JSValue mat4_static_fromTranslation (JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic) {
    const auto it = g_mat4AdapterInstances.find (magic);

    if (it == g_mat4AdapterInstances.end ()) {
        return JS_ThrowInternalError (ctx, "Mat4 adapter is no longer available");
    }

    glm::vec3 v {};

    if (argc < 1 || !tryGetVec2or3 (ctx, argv[0], v)) {
        return JS_ThrowTypeError (ctx, "fromTranslation() requires a Vec2 or Vec3 argument");
    }

    return it->second.wrap (glm::translate (glm::mat4 (1.0f), v));
}

JSValue mat4_static_fromScale (JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic) {
    const auto it = g_mat4AdapterInstances.find (magic);

    if (it == g_mat4AdapterInstances.end ()) {
        return JS_ThrowInternalError (ctx, "Mat4 adapter is no longer available");
    }

    glm::vec3 v {};

    if (argc < 1 || !tryGetNumberOrVec3 (ctx, argv[0], v)) {
        return JS_ThrowTypeError (ctx, "fromScale() requires a Number or Vec3 argument");
    }

    return it->second.wrap (glm::scale (glm::mat4 (1.0f), v));
}

JSValue mat4_static_fromRotation (JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic) {
    const auto it = g_mat4AdapterInstances.find (magic);

    if (it == g_mat4AdapterInstances.end ()) {
        return JS_ThrowInternalError (ctx, "Mat4 adapter is no longer available");
    }

    glm::vec3 axis {};

    if (argc < 2 || !JS_IsNumber (argv[0]) || !tryGetVec3 (ctx, argv[1], axis)) {
        return JS_ThrowTypeError (ctx, "fromRotation() requires (angle: Number, axis: Vec3)");
    }

    double angle = 0.0;
    JS_ToFloat64 (ctx, &angle, argv[0]);

    return it->second.wrap (glm::rotate (glm::mat4 (1.0f), glm::radians (static_cast<float> (angle)), axis));
}

JSValue mat4_static_fromEuler (JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic) {
    const auto it = g_mat4AdapterInstances.find (magic);

    if (it == g_mat4AdapterInstances.end ()) {
        return JS_ThrowInternalError (ctx, "Mat4 adapter is no longer available");
    }

    if (argc < 1) {
        return JS_ThrowTypeError (ctx, "fromEuler() requires at least 1 argument");
    }

    glm::vec3 euler {};

    if (JS_IsNumber (argv[0])) {
        double x = 0.0, y = 0.0, z = 0.0;
        JS_ToFloat64 (ctx, &x, argv[0]);

        if (argc > 1) {
            JS_ToFloat64 (ctx, &y, argv[1]);
        }
        if (argc > 2) {
            JS_ToFloat64 (ctx, &z, argv[2]);
        }

        euler = glm::vec3 (x, y, z);
    } else if (!tryGetVec3 (ctx, argv[0], euler)) {
        return JS_ThrowTypeError (ctx, "fromEuler() requires a Vec3, or 1-3 Number arguments");
    }

    const glm::vec3 radians = glm::radians (euler);

    return it->second.wrap (glm::eulerAngleXYZ (radians.x, radians.y, radians.z));
}

JSValue mat4_static_fromBasis (JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic) {
    const auto it = g_mat4AdapterInstances.find (magic);

    if (it == g_mat4AdapterInstances.end ()) {
        return JS_ThrowInternalError (ctx, "Mat4 adapter is no longer available");
    }

    glm::vec3 right {}, up {}, forward {};

    if (argc < 3 || !tryGetVec3 (ctx, argv[0], right) || !tryGetVec3 (ctx, argv[1], up)
        || !tryGetVec3 (ctx, argv[2], forward)) {
        return JS_ThrowTypeError (ctx, "fromBasis() requires (right: Vec3, up: Vec3, forward: Vec3)");
    }

    glm::mat4 result (1.0f);
    result[0] = glm::vec4 (right, 0.0f);
    result[1] = glm::vec4 (up, 0.0f);
    result[2] = glm::vec4 (forward, 0.0f);

    return it->second.wrap (result);
}

JSValue mat4_static_lookAt (JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic) {
    const auto it = g_mat4AdapterInstances.find (magic);

    if (it == g_mat4AdapterInstances.end ()) {
        return JS_ThrowInternalError (ctx, "Mat4 adapter is no longer available");
    }

    glm::vec3 eye {}, center {}, up {};

    if (argc < 3 || !tryGetVec3 (ctx, argv[0], eye) || !tryGetVec3 (ctx, argv[1], center)
        || !tryGetVec3 (ctx, argv[2], up)) {
        return JS_ThrowTypeError (ctx, "lookAt() requires (eye: Vec3, center: Vec3, up: Vec3)");
    }

    return it->second.wrap (glm::lookAt (eye, center, up));
}

JSValue mat4_static_compose (JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic) {
    const auto it = g_mat4AdapterInstances.find (magic);

    if (it == g_mat4AdapterInstances.end ()) {
        return JS_ThrowInternalError (ctx, "Mat4 adapter is no longer available");
    }

    glm::vec3 translation {}, rotation {}, scale {};

    if (argc < 3 || !tryGetVec3 (ctx, argv[0], translation) || !tryGetVec3 (ctx, argv[1], rotation)
        || !tryGetVec3 (ctx, argv[2], scale)) {
        return JS_ThrowTypeError (ctx, "compose() requires (translation: Vec3, rotation: Vec3, scale: Vec3)");
    }

    const glm::vec3 radians = glm::radians (rotation);
    const glm::mat4 t = glm::translate (glm::mat4 (1.0f), translation);
    const glm::mat4 r = glm::eulerAngleXYZ (radians.x, radians.y, radians.z);
    const glm::mat4 s = glm::scale (glm::mat4 (1.0f), scale);

    return it->second.wrap (t * r * s);
}

// ---------------------------------------------------------------------------------------------
// Instance (prototype) methods — resolve the adapter directly via container->adapter, exactly
// like VectorAdapter's instance methods do (no magic lookup needed once an instance exists).
// ---------------------------------------------------------------------------------------------

JSValue mat4_translation (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    if (argc >= 1 && !JS_IsUndefined (argv[0])) {
        glm::vec3 v {};

        if (!tryGetVec2or3 (ctx, argv[0], v)) {
            return JS_ThrowTypeError (ctx, "translation() requires a Vec2 or Vec3 argument");
        }

        container->value[3] = glm::vec4 (v, 1.0f);
    }

    return makeVec3 (container->adapter.getEngine (), glm::vec3 (container->value[3]));
}

JSValue mat4_right (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    return makeVec3 (container->adapter.getEngine (), glm::vec3 (container->value[0]));
}

JSValue mat4_up (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    return makeVec3 (container->adapter.getEngine (), glm::vec3 (container->value[1]));
}

JSValue mat4_forward (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    return makeVec3 (container->adapter.getEngine (), glm::vec3 (container->value[2]));
}

JSValue mat4_add (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    auto* other = argc >= 1 ? getContainer (argv[0]) : nullptr;

    if (!other) {
        return JS_ThrowTypeError (ctx, "add() requires a Mat4 argument");
    }

    return container->adapter.wrap (container->value + other->value);
}

JSValue mat4_subtract (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    auto* other = argc >= 1 ? getContainer (argv[0]) : nullptr;

    if (!other) {
        return JS_ThrowTypeError (ctx, "subtract() requires a Mat4 argument");
    }

    return container->adapter.wrap (container->value - other->value);
}

JSValue mat4_multiply (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    if (argc < 1) {
        return JS_ThrowTypeError (ctx, "multiply() requires a Mat4, Vec4, or Number argument");
    }

    if (JS_IsNumber (argv[0])) {
        double scalar = 0.0;
        JS_ToFloat64 (ctx, &scalar, argv[0]);

        return container->adapter.wrap (container->value * static_cast<float> (scalar));
    }

    if (auto* other = getContainer (argv[0])) {
        return container->adapter.wrap (container->value * other->value);
    }

    glm::vec4 v {};

    if (tryGetVec4 (ctx, argv[0], v)) {
        return makeVec4 (container->adapter.getEngine (), container->value * v);
    }

    return JS_ThrowTypeError (ctx, "multiply() requires a Mat4, Vec4, or Number argument");
}

JSValue mat4_translate (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    glm::vec3 v {};

    if (argc < 1 || !tryGetVec2or3 (ctx, argv[0], v)) {
        return JS_ThrowTypeError (ctx, "translate() requires a Vec2 or Vec3 argument");
    }

    return container->adapter.wrap (container->value * glm::translate (glm::mat4 (1.0f), v));
}

JSValue mat4_rotate (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    glm::vec3 axis {};

    if (argc < 2 || !JS_IsNumber (argv[0]) || !tryGetVec3 (ctx, argv[1], axis)) {
        return JS_ThrowTypeError (ctx, "rotate() requires (angle: Number, axis: Vec3)");
    }

    double angle = 0.0;
    JS_ToFloat64 (ctx, &angle, argv[0]);

    return container->adapter.wrap (
        container->value * glm::rotate (glm::mat4 (1.0f), glm::radians (static_cast<float> (angle)), axis)
    );
}

JSValue mat4_scale (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    glm::vec3 v {};

    if (argc < 1 || !tryGetNumberOrVec3 (ctx, argv[0], v)) {
        return JS_ThrowTypeError (ctx, "scale() requires a Number or Vec3 argument");
    }

    return container->adapter.wrap (container->value * glm::scale (glm::mat4 (1.0f), v));
}

JSValue mat4_transformPoint (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    glm::vec3 v {};

    if (argc < 1 || !tryGetVec3 (ctx, argv[0], v)) {
        return JS_ThrowTypeError (ctx, "transformPoint() requires a Vec3 argument");
    }

    return makeVec3 (container->adapter.getEngine (), glm::vec3 (container->value * glm::vec4 (v, 1.0f)));
}

JSValue mat4_transformDirection (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    glm::vec3 v {};

    if (argc < 1 || !tryGetVec3 (ctx, argv[0], v)) {
        return JS_ThrowTypeError (ctx, "transformDirection() requires a Vec3 argument");
    }

    return makeVec3 (container->adapter.getEngine (), glm::vec3 (container->value * glm::vec4 (v, 0.0f)));
}

JSValue mat4_transpose (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    return container->adapter.wrap (glm::transpose (container->value));
}

JSValue mat4_inverse (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    return container->adapter.wrap (glm::inverse (container->value));
}

JSValue mat4_determinant (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    return JS_NewFloat64 (ctx, glm::determinant (container->value));
}

// Shared by extractEuler() and decompose(): strips scale from the upper-left 3x3 by normalizing
// its columns, then extracts XYZ Euler angles (radians) via GLM's matching eulerAngleXYZ inverse.
glm::vec3 extractEulerRadians (const glm::mat4& m) {
    glm::vec3 c0 (m[0]), c1 (m[1]), c2 (m[2]);
    const float l0 = glm::length (c0), l1 = glm::length (c1), l2 = glm::length (c2);

    if (l0 > kMat4Epsilon) {
        c0 /= l0;
    }
    if (l1 > kMat4Epsilon) {
        c1 /= l1;
    }
    if (l2 > kMat4Epsilon) {
        c2 /= l2;
    }

    glm::mat4 rotation (1.0f);
    rotation[0] = glm::vec4 (c0, 0.0f);
    rotation[1] = glm::vec4 (c1, 0.0f);
    rotation[2] = glm::vec4 (c2, 0.0f);

    float x = 0.0f, y = 0.0f, z = 0.0f;
    glm::extractEulerAngleXYZ (rotation, x, y, z);

    return { x, y, z };
}

JSValue mat4_extractEuler (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    return makeVec3 (container->adapter.getEngine (), glm::degrees (extractEulerRadians (container->value)));
}

JSValue mat4_normalMatrix (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    const glm::mat3 normalMatrix = glm::transpose (glm::inverse (glm::mat3 (container->value)));

    return container->adapter.getEngine ().getAdapters ().mat3->wrap (normalMatrix);
}

JSValue mat4_decompose (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    const glm::vec3 translation (container->value[3]);

    glm::vec3 c0 (container->value[0]), c1 (container->value[1]), c2 (container->value[2]);
    const glm::vec3 scale (glm::length (c0), glm::length (c1), glm::length (c2));

    const glm::vec3 rotationDegrees = glm::degrees (extractEulerRadians (container->value));

    JSValue result = JS_NewObject (ctx);
    JS_SetPropertyStr (ctx, result, "translation", makeVec3 (container->adapter.getEngine (), translation));
    JS_SetPropertyStr (ctx, result, "rotation", makeVec3 (container->adapter.getEngine (), rotationDegrees));
    JS_SetPropertyStr (ctx, result, "scale", makeVec3 (container->adapter.getEngine (), scale));

    return result;
}

JSValue mat4_copy (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    return container->adapter.wrap (container->value);
}

JSValue mat4_equals (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    auto* other = argc >= 1 ? getContainer (argv[0]) : nullptr;

    if (!other) {
        return JS_ThrowTypeError (ctx, "equals() requires a Mat4 argument");
    }

    const float* a = glm::value_ptr (container->value);
    const float* b = glm::value_ptr (other->value);
    bool equal = true;

    for (int i = 0; i < 16 && equal; i++) {
        equal = std::fabs (a[i] - b[i]) <= kMat4Epsilon;
    }

    return JS_NewBool (ctx, equal);
}

JSValue mat4_toString (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* container = getContainer (this_val);
    MAT4_MAGIC_CHECK_EXCEPTION (container);

    return JS_NewString (ctx, formatComponents (glm::value_ptr (container->value), 16).c_str ());
}

} // namespace

Mat4Adapter::Mat4Adapter (ScriptEngine& engine) :
    ObjectAdapter (engine), m_name ("Mat4"), m_instanceId (++g_mat4AdapterInstanceId) {
    g_mat4AdapterInstances.emplace (this->m_instanceId, *this);

    this->registerType ({
        .class_name = this->m_name.c_str (),
        .finalizer = [] (JSRuntime*, JSValueConst val) {
            JSClassID classId = 0;
            auto* container = static_cast<Mat4OpaqueContainer*> (JS_GetAnyOpaque (val, &classId));

            if (container != nullptr && container->magic == kMat4OpaqueMagic) {
                delete container;
            }
        },
    });

    m_prototype = JS_NewObject (this->m_engine.getContext ());
    JS_DupValue (this->m_engine.getContext (), m_prototype);

    JSValue ctor = JS_NewCFunctionMagic (
        this->m_engine.getContext (), mat4_constructor, this->m_name.c_str (), 0, JS_CFUNC_constructor_magic,
        this->m_instanceId
    );

    JS_SetConstructor (this->m_engine.getContext (), ctor, m_prototype);

    JS_DefinePropertyGetSet (
        this->m_engine.getContext (), m_prototype, JS_NewAtom (this->m_engine.getContext (), "m"),
        JS_NewCFunction (this->m_engine.getContext (), mat4_get_m, "get", 0),
        JS_NewCFunction (this->m_engine.getContext (), mat4_set_m, "set", 1), JS_PROP_ENUMERABLE
    );

    const struct {
        const char* name;
        JSCFunction* func;
        int length;
    } methods[] = {
        { "translation", mat4_translation, 0 },
        { "right", mat4_right, 0 },
        { "up", mat4_up, 0 },
        { "forward", mat4_forward, 0 },
        { "add", mat4_add, 1 },
        { "subtract", mat4_subtract, 1 },
        { "multiply", mat4_multiply, 1 },
        { "translate", mat4_translate, 1 },
        { "rotate", mat4_rotate, 2 },
        { "scale", mat4_scale, 1 },
        { "transformPoint", mat4_transformPoint, 1 },
        { "transformDirection", mat4_transformDirection, 1 },
        { "transpose", mat4_transpose, 0 },
        { "inverse", mat4_inverse, 0 },
        { "determinant", mat4_determinant, 0 },
        { "extractEuler", mat4_extractEuler, 0 },
        { "normalMatrix", mat4_normalMatrix, 0 },
        { "decompose", mat4_decompose, 0 },
        { "copy", mat4_copy, 0 },
        { "equals", mat4_equals, 1 },
        { "toString", mat4_toString, 0 },
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
        { "identity", mat4_static_identity, 0 },
        { "fromTranslation", mat4_static_fromTranslation, 1 },
        { "fromScale", mat4_static_fromScale, 1 },
        { "fromRotation", mat4_static_fromRotation, 2 },
        { "fromEuler", mat4_static_fromEuler, 3 },
        { "fromBasis", mat4_static_fromBasis, 3 },
        { "lookAt", mat4_static_lookAt, 3 },
        { "compose", mat4_static_compose, 3 },
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

Mat4Adapter::~Mat4Adapter () { g_mat4AdapterInstances.erase (this->m_instanceId); }

void Mat4Adapter::releaseBeforeRuntimeShutdown () {
    JS_FreeValue (this->m_engine.getContext (), m_prototype);
    m_prototype = JS_UNDEFINED;
}

JSValue Mat4Adapter::wrap (const glm::mat4& value) {
    JSValue result = JS_NewObjectClass (this->m_engine.getContext (), this->m_classId);
    JS_SetOpaque (result, new Mat4OpaqueContainer { kMat4OpaqueMagic, *this, value });

    return result;
}

bool Mat4Adapter::extract (JSValueConst value, glm::mat4& out) const {
    auto* container = getContainer (value);

    if (container == nullptr) {
        return false;
    }

    out = container->value;

    return true;
}
