#include "VectorAdapter.h"

#include "../ScriptEngine.h"
#include "WallpaperEngine/Data/Utils/SFINAE.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"

#include <algorithm>
#include <cmath>
#include <variant>

using namespace WallpaperEngine::Data::Utils;
using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::Scripting::Adapters;

static uint32_t VectorInstanceId = 0;
static uint32_t VectorAdapterInstanceId = 0;
static constexpr int InvalidVectorInstanceId = 0;

// magic value used to ensure the assigned opaque value we got back is valid
#define VEC_OPAQUE_MAGIC 0xdeadbee0
#define VEC_MAGIC_CHECK_EXCEPTION(container, components)                                                               \
    do {                                                                                                               \
	if (!container || container->magic != (int)(VEC_OPAQUE_MAGIC + components)) {                                  \
	    return JS_ThrowTypeError (ctx, "not a Vec%d object", (int) (components));                                  \
	}                                                                                                              \
    } while (0)
#define VEC_MAGIC_CHECK_ERROR(container, components)                                                                   \
    do {                                                                                                               \
	if (!container || container->magic != (int)(VEC_OPAQUE_MAGIC + components)) {                                  \
	    JS_ThrowTypeError (ctx, "not a Vec%d object", (int) (components));                                         \
	    return -1;                                                                                                 \
	}                                                                                                              \
    } while (0)

template <int components> std::map<uint32_t, VectorAdapter<components>&> vectorAdapterInstances;

template <int components> struct VectorOpaqueContainer {
    int magic;
    VectorAdapter<components>& adapter;
    DynamicValue& value;
    uint32_t id;
};

template <int components> auto vector_new () -> decltype (auto) {
    static_assert (components >= 2 && components <= 4, "Unsupported vector type");

    if constexpr (components == 2) {
	return glm::vec2 {};
    } else if constexpr (components == 3) {
	return glm::vec3 {};
    } else if constexpr (components == 4) {
	return glm::vec4 {};
    }
}

template auto vector_new<2> () -> decltype (auto);
template auto vector_new<3> () -> decltype (auto);
template auto vector_new<4> () -> decltype (auto);

template <int components> auto vector_new (float value) -> decltype (auto) {
    static_assert (components >= 2 && components <= 4, "Unsupported vector type");

    if constexpr (components == 2) {
	return glm::vec2 (value);
    } else if constexpr (components == 3) {
	return glm::vec3 (value);
    } else if constexpr (components == 4) {
	return glm::vec4 (value);
    }
}

template auto vector_new<2> (float value) -> decltype (auto);
template auto vector_new<3> (float value) -> decltype (auto);
template auto vector_new<4> (float value) -> decltype (auto);

template <int components> auto vector_get (DynamicValue& value) -> decltype (auto) {
    static_assert (components >= 2 && components <= 4, "Unsupported vector type");

    if constexpr (components == 2) {
	return value.getVec2 ();
    } else if constexpr (components == 3) {
	return value.getVec3 ();
    } else if constexpr (components == 4) {
	return value.getVec4 ();
    }
}

template <int components> auto vector_get (JSContext* ctx, JSValue source) -> decltype (auto) {
    static_assert (components >= 2 && components <= 4, "Unsupported vector type");

    int tag = JS_VALUE_GET_TAG (source);

    if (tag == JS_TAG_INT) {
	int32_t value = 0;

	JS_ToInt32 (ctx, &value, source);

	return vector_new<components> (value);
    }

    if (JS_TAG_IS_FLOAT64 (tag)) {
	double value = 0.0f;

	JS_ToFloat64 (ctx, &value, source);

	return vector_new<components> (static_cast<float> (value));
    }

    if (tag == JS_TAG_OBJECT) {
	// check components, extract x, y, z and w and create the appropriate vector
	JSValue x = JS_GetPropertyStr (ctx, source, "x");
	JSValue y = JS_GetPropertyStr (ctx, source, "y");
	JSValue z = JS_GetPropertyStr (ctx, source, "z");
	JSValue w = JS_GetPropertyStr (ctx, source, "w");

	if (!JS_IsNumber (x) || !JS_IsNumber (y)) {
	    throw std::runtime_error ("Unsupported type conversion for VectorAdapter");
	}

	// do not accept bigger vectors
	if (components <= 2 && JS_IsNumber (z)) {
	    throw std::runtime_error ("Unsupported type conversion for VectorAdapter");
	}

	if (components <= 3 && JS_IsNumber (w)) {
	    throw std::runtime_error ("Unsupported type conversion for VectorAdapter");
	}

	double xVal = 0.0f, yVal = 0.0f, zVal = 0.0f, wVal = 0.0f;

	JS_ToFloat64 (ctx, &xVal, x);
	JS_ToFloat64 (ctx, &yVal, y);

	if (JS_IsNumber (z)) {
	    JS_ToFloat64 (ctx, &zVal, z);
	}

	if (JS_IsNumber (w)) {
	    JS_ToFloat64 (ctx, &wVal, w);
	}

	if constexpr (components == 2) {
	    return glm::vec2 (xVal, yVal);
	} else if constexpr (components == 3) {
	    return glm::vec3 (xVal, yVal, zVal);
	} else if constexpr (components == 4) {
	    return glm::vec4 (xVal, yVal, zVal, wVal);
	}
    }

    throw std::runtime_error ("Unsupported type conversion for VectorAdapter");
}

template auto vector_get<2> (JSContext* ctx, JSValue source) -> decltype (auto);
template auto vector_get<3> (JSContext* ctx, JSValue source) -> decltype (auto);
template auto vector_get<4> (JSContext* ctx, JSValue source) -> decltype (auto);
template auto vector_get<2> (DynamicValue& value) -> decltype (auto);
template auto vector_get<3> (DynamicValue& value) -> decltype (auto);
template auto vector_get<4> (DynamicValue& value) -> decltype (auto);

template <int components>
JSValue vector_property_get (JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst receiver) {
    JSClassID classId = 0;

    auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (obj_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    const char* name = JS_AtomToCString (ctx, atom);

    if (name == nullptr) {
	return JS_EXCEPTION;
    }

    ScopeGuard guard ([=] { JS_FreeCString (ctx, name); });
    const auto value = vector_get<components> (container->value);

    if (strcmp (name, "x") == 0) {
	return JS_NewFloat64 (ctx, value.x);
    }
    if (strcmp (name, "y") == 0) {
	return JS_NewFloat64 (ctx, value.y);
    }
    if constexpr (components >= 3) {
	if (strcmp (name, "z") == 0) {
	    return JS_NewFloat64 (ctx, value.z);
	}

	if constexpr (components >= 4) {
	    if (strcmp (name, "w") == 0) {
		return JS_NewFloat64 (ctx, value.w);
	    }
	}
    }

    // Not one of the vector components: this exotic getter otherwise shadows the whole prototype
    // chain (QuickJS never falls back to it once a class defines get_property), so resolve
    // anything else — prototype methods like length()/add()/toString(), Symbol.toPrimitive, etc.
    // — directly against the class prototype instead.
    JSValue proto = JS_GetClassProto (ctx, classId);

    if (JS_IsException (proto) || JS_IsUndefined (proto)) {
	return JS_UNDEFINED;
    }

    JSValue result = JS_GetProperty (ctx, proto, atom);

    JS_FreeValue (ctx, proto);

    return result;
}

template JSValue vector_property_get<2> (JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst receiver);
template JSValue vector_property_get<3> (JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst receiver);
template JSValue vector_property_get<4> (JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst receiver);

template <int components>
int vector_property_set (
    JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst val, JSValueConst receiver, int flags
) {
    JSClassID classId = 0;
    auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (obj_val, &classId));

    VEC_MAGIC_CHECK_ERROR (container, components);

    int tag = JS_VALUE_GET_TAG (val);

    if (tag != JS_TAG_INT && !JS_TAG_IS_FLOAT64 (tag)) {
	JS_ThrowTypeError (ctx, "cannot assign a non-numeric value to a Vec%d component", components);
	return -1;
    }

    const char* name = JS_AtomToCString (ctx, atom);

    if (name == nullptr) {
	return -1;
    }

    ScopeGuard guard ([=] { JS_FreeCString (ctx, name); });
    auto vec = vector_get<components> (container->value);
    void* into = nullptr;

    if (strcmp (name, "x") == 0) {
	if constexpr (
	    std::is_same_v<decltype (vec), glm::vec2> || std::is_same_v<decltype (vec), glm::vec3>
	    || std::is_same_v<decltype (vec), glm::vec4>
	) {
	    into = &vec.x;
	} else if constexpr (std::is_same_v<decltype (vec), Color>) {
	    into = &vec.r;
	}
    } else if (strcmp (name, "y") == 0) {
	if constexpr (
	    std::is_same_v<decltype (vec), glm::vec2> || std::is_same_v<decltype (vec), glm::vec3>
	    || std::is_same_v<decltype (vec), glm::vec4>
	) {
	    into = &vec.y;
	} else if constexpr (std::is_same_v<decltype (vec), Color>) {
	    into = &vec.g;
	}
    } else if constexpr (components >= 3) {
	if (strcmp (name, "z") == 0) {
	    if constexpr (std::is_same_v<decltype (vec), glm::vec3> || std::is_same_v<decltype (vec), glm::vec4>) {
		into = &vec.z;
	    } else if constexpr (std::is_same_v<decltype (vec), Color>) {
		into = &vec.b;
	    }
	} else if constexpr (components >= 4) {
	    if (strcmp (name, "w") == 0) {
		if constexpr (std::is_same_v<decltype (vec), glm::vec4>) {
		    into = &vec.w;
		} else if constexpr (std::is_same_v<decltype (vec), Color>) {
		    into = &vec.a;
		}
	    }
	}
    }

    if (into == nullptr) {
	JS_ThrowTypeError (ctx, "Vec%d has no writable property '%s'", components, name);
	return -1;
    }

    double value = 0;

    JS_ToFloat64 (ctx, &value, val);

    *static_cast<float*> (into) = static_cast<float> (value);

    container->value.update (vec, DynamicValue::UpdateSource::Script);

    return 0;
}

template int vector_property_set<2> (
    JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst val, JSValueConst receiver, int flags
);
template int vector_property_set<3> (
    JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst val, JSValueConst receiver, int flags
);
template int vector_property_set<4> (
    JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst val, JSValueConst receiver, int flags
);

template <int components> JSValue vector_copy (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;

    auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    // create a new DynamicValue
    return container->adapter.instantiate (container->value, true);
}

template JSValue vector_copy<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_copy<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_copy<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

bool vector_value_equals (JSContext* ctx, JSValue left, JSValue right) {
    if (JS_TAG_IS_FLOAT64 (JS_VALUE_GET_TAG (left)) || JS_TAG_IS_FLOAT64 (JS_VALUE_GET_TAG (right))) {
	double x1Val = 0.0f, x2Val = 0.0f;

	JS_ToFloat64 (ctx, &x1Val, left);
	JS_ToFloat64 (ctx, &x2Val, right);

	if (std::abs (x1Val - x2Val) > 0.00001) {
	    return false;
	}
    } else {
	if (!JS_IsEqual (ctx, left, right)) {
	    return false;
	}
    }

    return true;
}

template <int components> JSValue vector_equals (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc == 0) {
	return JS_FALSE;
    }

    JSClassID classId = 0;

    auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue other = argv[0];
    auto* otherContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (other, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (otherContainer, components);

    const auto vector = vector_get<components> (container->value);
    const auto otherVector = vector_get<components> (otherContainer->value);

    if constexpr (components == 2) {
	return vector.x == otherVector.x && vector.y == otherVector.y ? JS_TRUE : JS_FALSE;
    } else if constexpr (components == 3) {
	return vector.x == otherVector.x && vector.y == otherVector.y && vector.z == otherVector.z ? JS_TRUE : JS_FALSE;
    } else if constexpr (components == 4) {
	return vector.x == otherVector.x && vector.y == otherVector.y && vector.z == otherVector.z
		&& vector.w == otherVector.w
	    ? JS_TRUE
	    : JS_FALSE;
    }

    return JS_FALSE;
}

template JSValue vector_equals<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_equals<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_equals<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_length (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;

    auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    return JS_NewFloat64 (ctx, glm::length (vector_get<components> (container->value)));
}

template JSValue vector_length<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_length<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_length<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components>
JSValue vector_constructor (JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv, int magic) {
    if (argc == 0) {
	return JS_ThrowTypeError (ctx, "Vec%d constructor requires at least 1 argument", components);
    }

    auto it = vectorAdapterInstances<components>.find (magic);

    if (it == vectorAdapterInstances<components>.end ()) {
	return JS_ThrowInternalError (ctx, "Vec%d adapter is no longer available", components);
    }

    JSValue result = it->second.instantiate ();
    JSClassID classId = 0;
    auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (result, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    // `new Vec3(x, y, z)` / `new Vec2(x, y)` with one numeric argument per component is the
    // common case in real scripts — handle it explicitly instead of falling through to
    // vector_get(), which only ever looks at argv[0] and would silently broadcast just the first
    // component (e.g. turning `new Vec3(1, 2, 3)` into (1, 1, 1)).
    bool allNumeric = argc >= components;

    for (int i = 0; allNumeric && i < components; i++) {
	allNumeric = JS_IsNumber (argv[i]);
    }

    if (allNumeric) {
	double x = 0.0, y = 0.0, z = 0.0, w = 0.0;

	JS_ToFloat64 (ctx, &x, argv[0]);
	JS_ToFloat64 (ctx, &y, argv[1]);

	if constexpr (components >= 3) {
	    JS_ToFloat64 (ctx, &z, argv[2]);
	}
	if constexpr (components >= 4) {
	    JS_ToFloat64 (ctx, &w, argv[3]);
	}

	if constexpr (components == 2) {
	    container->value.update (glm::vec2 (x, y), DynamicValue::UpdateSource::Initialization);
	} else if constexpr (components == 3) {
	    container->value.update (glm::vec3 (x, y, z), DynamicValue::UpdateSource::Initialization);
	} else if constexpr (components == 4) {
	    container->value.update (glm::vec4 (x, y, z, w), DynamicValue::UpdateSource::Initialization);
	}
    } else {
	container->value.update (vector_get<components> (ctx, argv[0]), DynamicValue::UpdateSource::Initialization);
    }

    return result;
}

template JSValue vector_constructor<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);
template JSValue vector_constructor<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);
template JSValue vector_constructor<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);

template <int components> void vector_finalizer (JSRuntime* rt, JSValueConst val) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (val, &classId));

    if (!container || container->magic != (int)(VEC_OPAQUE_MAGIC + components)) {
	return;
    }

    // free container and the associated DynamicValue if temporal
    if (container->id != InvalidVectorInstanceId) {
	container->adapter.free (container->id);
    }

    delete container;
}

template void vector_finalizer<2> (JSRuntime* rt, JSValueConst val);
template void vector_finalizer<3> (JSRuntime* rt, JSValueConst val);
template void vector_finalizer<4> (JSRuntime* rt, JSValueConst val);

template <int components>
JSValue vector_normalize (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate (container->value, true);

    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    newContainer->value.update (
	glm::normalize (vector_get<components> (container->value)), DynamicValue::UpdateSource::Script
    );

    return newVector;
}

template JSValue vector_normalize<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_normalize<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_normalize<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_add (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_UNDEFINED;
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	vector_get<components> (ctx, argv[0]) + vector_get<components> (container->value),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_add<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_add<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_add<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components>
JSValue vector_subtract (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_UNDEFINED;
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	vector_get<components> (container->value) - vector_get<components> (ctx, argv[0]),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_subtract<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_subtract<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_subtract<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components>
JSValue vector_multiply (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_UNDEFINED;
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	vector_get<components> (ctx, argv[0]) * vector_get<components> (container->value),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_multiply<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_multiply<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_multiply<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_divide (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_UNDEFINED;
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	vector_get<components> (container->value) / vector_get<components> (ctx, argv[0]),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_divide<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_divide<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_divide<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_dot (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_UNDEFINED;
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	glm::dot (vector_get<components> (ctx, argv[0]), vector_get<components> (container->value)),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_dot<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_dot<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_dot<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_cross (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_UNDEFINED;
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	glm::cross (vector_get<components> (container->value), vector_get<components> (ctx, argv[0])),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_cross<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_mix (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 2) {
	return JS_ThrowTypeError (ctx, "mix() requires 2 arguments (vector, amount)");
    }

    if (!JS_IsNumber (argv[1])) {
	return JS_ThrowTypeError (ctx, "mix() amount must be a number");
    }

    double amount = 0.0f;

    JS_ToFloat64 (ctx, &amount, argv[1]);

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	glm::mix (vector_get<components> (container->value), vector_get<components> (ctx, argv[0]), amount),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_mix<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_mix<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_mix<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_min (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_UNDEFINED;
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	glm::min (vector_get<components> (ctx, argv[0]), vector_get<components> (container->value)),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_min<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_min<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_min<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_max (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_UNDEFINED;
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	glm::max (vector_get<components> (ctx, argv[0]), vector_get<components> (container->value)),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_max<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_max<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_max<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_abs (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	glm::abs (vector_get<components> (container->value)), DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_abs<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_abs<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_abs<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_sign (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	glm::sign (vector_get<components> (container->value)), DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_sign<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_sign<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_sign<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_round (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	glm::round (vector_get<components> (container->value)), DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_round<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_round<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_round<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_floor (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	glm::floor (vector_get<components> (container->value)), DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_floor<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_floor<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_floor<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_ceil (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));
    const auto vector = vector_get<components> (container->value);

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (glm::ceil (vector), DynamicValue::UpdateSource::Initialization);

    return newVector;
}

template JSValue vector_ceil<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_ceil<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_ceil<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components>
JSValue vector_toString (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    return JS_NewString (ctx, container->value.toString ().c_str ());
}

template JSValue vector_toString<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_toString<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_toString<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components>
JSValue vector_lengthSqr (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    const auto vector = vector_get<components> (container->value);

    return JS_NewFloat64 (ctx, glm::dot (vector, vector));
}

template JSValue vector_lengthSqr<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_lengthSqr<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_lengthSqr<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_distance (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_ThrowTypeError (ctx, "distance() requires 1 argument");
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    return JS_NewFloat64 (
	ctx, glm::distance (vector_get<components> (container->value), vector_get<components> (ctx, argv[0]))
    );
}

template JSValue vector_distance<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_distance<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_distance<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components>
JSValue vector_distanceSqr (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_ThrowTypeError (ctx, "distanceSqr() requires 1 argument");
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    const auto delta = vector_get<components> (container->value) - vector_get<components> (ctx, argv[0]);

    return JS_NewFloat64 (ctx, glm::dot (delta, delta));
}

template JSValue vector_distanceSqr<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_distanceSqr<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_distanceSqr<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_isFinite (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    const auto vector = vector_get<components> (container->value);
    bool finite = std::isfinite (vector.x) && std::isfinite (vector.y);

    if constexpr (components >= 3) {
	finite = finite && std::isfinite (vector.z);
    }
    if constexpr (components >= 4) {
	finite = finite && std::isfinite (vector.w);
    }

    return JS_NewBool (ctx, finite);
}

template JSValue vector_isFinite<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_isFinite<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_isFinite<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_negate (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	-vector_get<components> (container->value), DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_negate<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_negate<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_negate<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_reflect (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_ThrowTypeError (ctx, "reflect() requires 1 argument (the normal vector)");
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	glm::reflect (vector_get<components> (container->value), vector_get<components> (ctx, argv[0])),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_reflect<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_reflect<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_reflect<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_project (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_ThrowTypeError (ctx, "project() requires 1 argument");
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    const auto self = vector_get<components> (container->value);
    const auto onto = vector_get<components> (ctx, argv[0]);
    const auto scale = glm::dot (self, onto) / glm::dot (onto, onto);

    newContainer->value.update (onto * scale, DynamicValue::UpdateSource::Initialization);

    return newVector;
}

template JSValue vector_project<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_project<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_project<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components>
JSValue vector_angleBetween (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_ThrowTypeError (ctx, "angleBetween() requires 1 argument");
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    const auto self = vector_get<components> (container->value);
    const auto other = vector_get<components> (ctx, argv[0]);
    const double cosAngle = glm::dot (self, other) / (glm::length (self) * glm::length (other));

    return JS_NewFloat64 (ctx, glm::degrees (std::acos (std::clamp (cosAngle, -1.0, 1.0))));
}

template JSValue vector_angleBetween<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_angleBetween<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_angleBetween<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_clamp (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 2) {
	return JS_ThrowTypeError (ctx, "clamp() requires 2 arguments (min, max)");
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	glm::clamp (
	    vector_get<components> (container->value), vector_get<components> (ctx, argv[0]),
	    vector_get<components> (ctx, argv[1])
	),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_clamp<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_clamp<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_clamp<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_fract (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	glm::fract (vector_get<components> (container->value)), DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_fract<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_fract<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_fract<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_mod (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_ThrowTypeError (ctx, "mod() requires 1 argument");
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	glm::mod (vector_get<components> (container->value), vector_get<components> (ctx, argv[0])),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_mod<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_mod<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_mod<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components> JSValue vector_step (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_ThrowTypeError (ctx, "step() requires 1 argument");
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	glm::step (vector_get<components> (ctx, argv[0]), vector_get<components> (container->value)),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_step<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_step<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_step<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

template <int components>
JSValue vector_smoothStep (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 2) {
	return JS_ThrowTypeError (ctx, "smoothStep() requires 2 arguments");
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, components);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<components>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, components);

    newContainer->value.update (
	glm::smoothstep (
	    vector_get<components> (ctx, argv[0]), vector_get<components> (ctx, argv[1]),
	    vector_get<components> (container->value)
	),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template JSValue vector_smoothStep<2> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_smoothStep<3> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
template JSValue vector_smoothStep<4> (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Vec2-only: angle of this vector from the positive x-axis, in degrees.
JSValue vector2_angle (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<2>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, 2);

    const auto vector = vector_get<2> (container->value);

    return JS_NewFloat64 (ctx, glm::degrees (std::atan2 (vector.y, vector.x)));
}

// Vec2-only: rotate this vector by an angle in degrees.
JSValue vector2_rotate (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsNumber (argv[0])) {
	return JS_ThrowTypeError (ctx, "rotate() requires 1 numeric argument (angle in degrees)");
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<2>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, 2);

    double angle = 0.0;

    JS_ToFloat64 (ctx, &angle, argv[0]);

    const auto vector = vector_get<2> (container->value);
    const double rad = glm::radians (angle);
    const double cosA = std::cos (rad);
    const double sinA = std::sin (rad);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<2>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, 2);

    newContainer->value.update (
	glm::vec2 (vector.x * cosA - vector.y * sinA, vector.x * sinA + vector.y * cosA),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

// Vec2-only: a vector perpendicular to this one (rotated 90 degrees counter-clockwise).
JSValue vector2_perpendicular (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<2>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, 2);

    const auto vector = vector_get<2> (container->value);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<2>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, 2);

    newContainer->value.update (glm::vec2 (-vector.y, vector.x), DynamicValue::UpdateSource::Initialization);

    return newVector;
}

// Vec3-only: refract this vector (the incident direction) against a normal, given an eta ratio.
JSValue vector3_refract (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 2 || !JS_IsNumber (argv[1])) {
	return JS_ThrowTypeError (ctx, "refract() requires 2 arguments (normal vector, eta number)");
    }

    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<3>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, 3);

    double eta = 0.0;

    JS_ToFloat64 (ctx, &eta, argv[1]);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<3>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, 3);

    newContainer->value.update (
	glm::refract (vector_get<3> (container->value), vector_get<3> (ctx, argv[0]), static_cast<float> (eta)),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

// Vec3-only: convert this cartesian vector to spherical coordinates (radius, polar angle, azimuth), angles in degrees.
JSValue vector3_toSpherical (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto* container = static_cast<VectorOpaqueContainer<3>*> (JS_GetAnyOpaque (this_val, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (container, 3);

    const auto vector = vector_get<3> (container->value);
    const double radius = glm::length (vector);

    JSValue newVector = container->adapter.instantiate ();
    const auto* newContainer = static_cast<VectorOpaqueContainer<3>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, 3);

    if (radius < 1e-10) {
	newContainer->value.update (glm::vec3 (0.0f, 0.0f, 0.0f), DynamicValue::UpdateSource::Initialization);

	return newVector;
    }

    const double theta = glm::degrees (std::acos (std::clamp (vector.y / radius, -1.0, 1.0)));
    const double phi = glm::degrees (std::atan2 (vector.z, vector.x));

    newContainer->value.update (
	glm::vec3 (radius, theta, phi), DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

// Vec3-only static: build a cartesian Vec3 from spherical coordinates (radius, polar angle, azimuth in degrees).
JSValue vector3_fromSpherical (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    if (argc != 3 || !JS_IsNumber (argv[0]) || !JS_IsNumber (argv[1]) || !JS_IsNumber (argv[2])) {
	return JS_ThrowTypeError (ctx, "fromSpherical() requires 3 numeric arguments (radius, theta, phi)");
    }

    const auto it = vectorAdapterInstances<3>.find (magic);

    if (it == vectorAdapterInstances<3>.end ()) {
	return JS_ThrowInternalError (ctx, "Vec3 adapter is no longer available");
    }

    double radius = 0.0, theta = 0.0, phi = 0.0;

    JS_ToFloat64 (ctx, &radius, argv[0]);
    JS_ToFloat64 (ctx, &theta, argv[1]);
    JS_ToFloat64 (ctx, &phi, argv[2]);

    const double t = glm::radians (theta);
    const double p = glm::radians (phi);

    JSValue newVector = it->second.instantiate ();
    JSClassID classId = 0;
    auto* newContainer = static_cast<VectorOpaqueContainer<3>*> (JS_GetAnyOpaque (newVector, &classId));

    VEC_MAGIC_CHECK_EXCEPTION (newContainer, 3);

    newContainer->value.update (
	glm::vec3 (
	    radius * std::sin (t) * std::cos (p), radius * std::cos (t), radius * std::sin (t) * std::sin (p)
	),
	DynamicValue::UpdateSource::Initialization
    );

    return newVector;
}

template <int components>
VectorAdapter<components>::VectorAdapter (ScriptEngine& engine) :
    ObjectAdapter (engine), m_instanceId (++VectorAdapterInstanceId), m_name ("Vec" + std::to_string (components)),
    m_exoticMethods (
	{
	    .get_property = vector_property_get<components>,
	    .set_property = vector_property_set<components>,
	}
    ) {
    vectorAdapterInstances<components>.emplace (this->m_instanceId, *this);
    this->registerType (
	{
	    .class_name = this->m_name.c_str (),
	    .finalizer = vector_finalizer<components>,
	    .exotic = &this->m_exoticMethods,
	}
    );

    // build the prototype for the Vector and assign the required methods
    m_prototype = JS_NewObject (this->m_engine.getContext ());

    JS_DupValue (this->m_engine.getContext (), m_prototype);

    JSValue ctor = JS_NewCFunctionMagic (
	this->m_engine.getContext (), vector_constructor<components>, this->m_name.c_str (), 1,
	JS_CFUNC_constructor_magic, this->m_instanceId
    );

    JS_SetConstructor (this->m_engine.getContext (), ctor, m_prototype);
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "copy",
	JS_NewCFunction (this->m_engine.getContext (), vector_copy<components>, "copy", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "equals",
	JS_NewCFunction (this->m_engine.getContext (), vector_equals<components>, "equals", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "length",
	JS_NewCFunction (this->m_engine.getContext (), vector_length<components>, "length", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "lengthSqr",
	JS_NewCFunction (this->m_engine.getContext (), vector_lengthSqr<components>, "lengthSqr", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "normalize",
	JS_NewCFunction (this->m_engine.getContext (), vector_normalize<components>, "normalize", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "add",
	JS_NewCFunction (this->m_engine.getContext (), vector_add<components>, "add", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "subtract",
	JS_NewCFunction (this->m_engine.getContext (), vector_subtract<components>, "subtract", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "multiply",
	JS_NewCFunction (this->m_engine.getContext (), vector_multiply<components>, "multiply", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "divide",
	JS_NewCFunction (this->m_engine.getContext (), vector_divide<components>, "divide", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "dot",
	JS_NewCFunction (this->m_engine.getContext (), vector_dot<components>, "dot", 1), JS_PROP_ENUMERABLE
    );
    if constexpr (components == 3) {
	JS_DefinePropertyValueStr (
	    this->m_engine.getContext (), m_prototype, "cross",
	    JS_NewCFunction (this->m_engine.getContext (), vector_cross<components>, "cross", 1), JS_PROP_ENUMERABLE
	);
    }
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "mix",
	JS_NewCFunction (this->m_engine.getContext (), vector_mix<components>, "mix", 2), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "min",
	JS_NewCFunction (this->m_engine.getContext (), vector_min<components>, "min", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "max",
	JS_NewCFunction (this->m_engine.getContext (), vector_max<components>, "max", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "abs",
	JS_NewCFunction (this->m_engine.getContext (), vector_abs<components>, "abs", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "sign",
	JS_NewCFunction (this->m_engine.getContext (), vector_sign<components>, "sign", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "round",
	JS_NewCFunction (this->m_engine.getContext (), vector_round<components>, "round", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "floor",
	JS_NewCFunction (this->m_engine.getContext (), vector_floor<components>, "floor", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "ceil",
	JS_NewCFunction (this->m_engine.getContext (), vector_ceil<components>, "ceil", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "toString",
	JS_NewCFunction (this->m_engine.getContext (), vector_toString<components>, "toString", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "distance",
	JS_NewCFunction (this->m_engine.getContext (), vector_distance<components>, "distance", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "distanceSqr",
	JS_NewCFunction (this->m_engine.getContext (), vector_distanceSqr<components>, "distanceSqr", 1),
	JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "isFinite",
	JS_NewCFunction (this->m_engine.getContext (), vector_isFinite<components>, "isFinite", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "negate",
	JS_NewCFunction (this->m_engine.getContext (), vector_negate<components>, "negate", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "reflect",
	JS_NewCFunction (this->m_engine.getContext (), vector_reflect<components>, "reflect", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "project",
	JS_NewCFunction (this->m_engine.getContext (), vector_project<components>, "project", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "angleBetween",
	JS_NewCFunction (this->m_engine.getContext (), vector_angleBetween<components>, "angleBetween", 1),
	JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "clamp",
	JS_NewCFunction (this->m_engine.getContext (), vector_clamp<components>, "clamp", 2), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "fract",
	JS_NewCFunction (this->m_engine.getContext (), vector_fract<components>, "fract", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "mod",
	JS_NewCFunction (this->m_engine.getContext (), vector_mod<components>, "mod", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "step",
	JS_NewCFunction (this->m_engine.getContext (), vector_step<components>, "step", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), m_prototype, "smoothStep",
	JS_NewCFunction (this->m_engine.getContext (), vector_smoothStep<components>, "smoothStep", 2),
	JS_PROP_ENUMERABLE
    );

    if constexpr (components == 2) {
	JS_DefinePropertyValueStr (
	    this->m_engine.getContext (), m_prototype, "angle",
	    JS_NewCFunction (this->m_engine.getContext (), vector2_angle, "angle", 0), JS_PROP_ENUMERABLE
	);
	JS_DefinePropertyValueStr (
	    this->m_engine.getContext (), m_prototype, "rotate",
	    JS_NewCFunction (this->m_engine.getContext (), vector2_rotate, "rotate", 1), JS_PROP_ENUMERABLE
	);
	JS_DefinePropertyValueStr (
	    this->m_engine.getContext (), m_prototype, "perpendicular",
	    JS_NewCFunction (this->m_engine.getContext (), vector2_perpendicular, "perpendicular", 0),
	    JS_PROP_ENUMERABLE
	);
    }

    if constexpr (components == 3) {
	JS_DefinePropertyValueStr (
	    this->m_engine.getContext (), m_prototype, "refract",
	    JS_NewCFunction (this->m_engine.getContext (), vector3_refract, "refract", 2), JS_PROP_ENUMERABLE
	);
	JS_DefinePropertyValueStr (
	    this->m_engine.getContext (), m_prototype, "toSpherical",
	    JS_NewCFunction (this->m_engine.getContext (), vector3_toSpherical, "toSpherical", 0), JS_PROP_ENUMERABLE
	);
	JS_DefinePropertyValueStr (
	    this->m_engine.getContext (), ctor, "fromSpherical",
	    JS_NewCFunctionMagic (
		this->m_engine.getContext (), vector3_fromSpherical, "fromSpherical", 3, JS_CFUNC_generic_magic,
		this->m_instanceId
	    ),
	    JS_PROP_ENUMERABLE
	);
    }

    JS_SetClassProto (this->m_engine.getContext (), this->m_classId, m_prototype);

    // expose the constructor on globalThis so scripts can do `new Vec2(x, y)` / `new Vec3(...)` / `new Vec4(...)`
    JS_SetPropertyStr (this->m_engine.getContext (), this->m_engine.getGlobalThis (), this->m_name.c_str (), ctor);
}

template <int components> void VectorAdapter<components>::releaseBeforeRuntimeShutdown () {
    JS_FreeValue (this->m_engine.getContext (), m_prototype);
    m_prototype = JS_UNDEFINED;
}

template <int components> VectorAdapter<components>::~VectorAdapter () {
    vectorAdapterInstances<components>.erase (this->m_instanceId);

    // releaseBeforeRuntimeShutdown() already released our reference to m_prototype, and
    // JS_FreeRuntime() has already torn down the context/runtime by the time this destructor
    // runs — there is nothing left to explicitly free here.
}

template <int components> JSValue VectorAdapter<components>::instantiate (ScriptableObject& object) {
    throw new std::runtime_error ("Cannot create a Vector4 instance from a ScriptableObject");
}

template <int components> JSValue VectorAdapter<components>::instantiate (DynamicValue& value) {
    JSValue result = this->ObjectAdapter::instantiate (value);
    JS_SetOpaque (
	result,
	new VectorOpaqueContainer<components> {
	    .magic = VEC_OPAQUE_MAGIC + components,
	    .adapter = *this,
	    .value = value,
	    .id = InvalidVectorInstanceId,
	}
    );

    return result;
}

template <int components> JSValue VectorAdapter<components>::instantiate (DynamicValue& source, bool temporal) {
    auto value = std::make_unique<DynamicValue> (source);
    uint32_t id = ++VectorInstanceId;
    JSValue result = this->ObjectAdapter::instantiate (*value);
    JS_SetOpaque (
	result,
	new VectorOpaqueContainer<components> {
	    .magic = VEC_OPAQUE_MAGIC + components,
	    .adapter = *this,
	    .value = *value,
	    .id = id,
	}
    );

    this->m_values.emplace (id, std::move (value));

    return result;
}

template <int components> JSValue VectorAdapter<components>::instantiate () {
    auto value = std::make_unique<DynamicValue> (vector_new<components> ());
    uint32_t id = ++VectorInstanceId;
    JSValue result = this->ObjectAdapter::instantiate (*value);
    JS_SetOpaque (
	result,
	new VectorOpaqueContainer<components> {
	    .magic = VEC_OPAQUE_MAGIC + components,
	    .adapter = *this,
	    .value = *value,
	    .id = id,
	}
    );

    this->m_values.emplace (id, std::move (value));

    return result;
}

template <int components> void VectorAdapter<components>::free (uint32_t vectorId) {
    auto it = this->m_values.find (vectorId);

    if (it != this->m_values.end ()) {
	this->m_values.erase (it);
    }
}

namespace WallpaperEngine::Scripting::Adapters {
template class VectorAdapter<2>;
template class VectorAdapter<3>;
template class VectorAdapter<4>;
} // namespace WallpaperEngine::Scripting::Adapters