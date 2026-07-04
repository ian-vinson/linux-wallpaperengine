#include "ScriptableObjectAdapter.h"

#include <cstring>
#include <stdexcept>
#include <utility>

#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Scripting/ScriptEngine.h"
#include "WallpaperEngine/Scripting/ScriptableObject.h"

using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::Data::Utils;
using namespace WallpaperEngine::Scripting::Adapters;

#define SCRIPTABLE_OPAQUE_MAGIC 0xdeadbeef

struct OpaqueScriptableObjectAdapter {
    unsigned int magic;
    ScriptableObjectAdapter& adapter;
    WallpaperEngine::Scripting::ScriptableObject& object;
};

// no-op stand-in for ITextureAnimation.play()/stop()/pause() — see scriptableobject_get_texture_animation.
JSValue scriptableobject_texture_animation_noop (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    return JS_UNDEFINED;
}

// thisLayer.getTextureAnimation(): real Wallpaper Engine returns an ITextureAnimation controller
// ({rate, play(), stop(), pause(), setFrame()}) that drives sprite-sheet playback. lwe's frame
// selection (CPass.cpp) picks the current frame from the scene's single global render clock
// modulo the texture's total animation duration — there is no per-object rate/pause/frame state
// anywhere in the rendering pipeline to hook into, and wiring one in would mean changing that
// shared timing logic for every renderable object, not just this API. So this returns an inert
// stub object: `.rate` is a plain read/write number with no effect on playback, and
// play/stop/pause/setFrame are no-ops. This is enough to stop `getTextureAnimation().stop()`-style
// calls from throwing "not a function" and crashing scripts that use them, but does not actually
// pause, seek, or change the speed of the rendered animation.
JSValue scriptableobject_get_texture_animation (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSValue anim = JS_NewObject (ctx);

    JS_SetPropertyStr (ctx, anim, "rate", JS_NewFloat64 (ctx, 1.0));
    JS_SetPropertyStr (
	ctx, anim, "play", JS_NewCFunction (ctx, scriptableobject_texture_animation_noop, "play", 0)
    );
    JS_SetPropertyStr (
	ctx, anim, "stop", JS_NewCFunction (ctx, scriptableobject_texture_animation_noop, "stop", 0)
    );
    JS_SetPropertyStr (
	ctx, anim, "pause", JS_NewCFunction (ctx, scriptableobject_texture_animation_noop, "pause", 0)
    );
    JS_SetPropertyStr (
	ctx, anim, "setFrame", JS_NewCFunction (ctx, scriptableobject_texture_animation_noop, "setFrame", 1)
    );

    return anim;
}

// thisLayer.getChildren(): every other scene object whose Object::parent
// references this object's id, in scene render order. Mirrors
// scene_enumerate_layers()'s iterate/filter/wrap shape (SceneObject.cpp) —
// skips objects that aren't ScriptableObject-derived (e.g. isPureGroup
// placeholder objects), since only those can be represented as an ILayer
// JS proxy.
JSValue scriptableobject_get_children (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (this_val, &classId));

    if (!container || container->magic != SCRIPTABLE_OPAQUE_MAGIC) {
	return JS_ThrowTypeError (ctx, "getChildren() called on an invalid receiver");
    }

    JSValue array = JS_NewArray (ctx);
    uint32_t index = 0;
    const int parentId = container->object.getId ();

    for (auto* candidate : container->object.getScene ().getObjectsByRenderOrder ()) {
	const auto& parent = candidate->getObject ().parent;

	if (!parent.has_value () || parent.value () != parentId) {
	    continue;
	}

	if (!candidate->is<WallpaperEngine::Scripting::ScriptableObject> ()) {
	    continue;
	}

	JS_SetPropertyUint32 (
	    ctx, array, index++,
	    container->adapter.instantiate (*candidate->as<WallpaperEngine::Scripting::ScriptableObject> ())
	);
    }

    return array;
}

// thisLayer.getParent(): resolves this object's Object::parent id back to
// a single CObject via the same getObjectsByRenderOrder() scan
// getChildren() uses (CScene::getObject(id) returns a const CObject*,
// which can't satisfy instantiate()'s non-const ScriptableObject&
// requirement — this sidesteps that entirely). Returns JS_UNDEFINED if
// unparented, or if the parent isn't ScriptableObject-derived.
JSValue scriptableobject_get_parent (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (this_val, &classId));

    if (!container || container->magic != SCRIPTABLE_OPAQUE_MAGIC) {
	return JS_ThrowTypeError (ctx, "getParent() called on an invalid receiver");
    }

    const auto& parent = container->object.getObject ().parent;

    if (!parent.has_value ()) {
	return JS_UNDEFINED;
    }

    const int parentId = parent.value ();

    for (auto* candidate : container->object.getScene ().getObjectsByRenderOrder ()) {
	if (candidate->getId () != parentId) {
	    continue;
	}

	if (!candidate->is<WallpaperEngine::Scripting::ScriptableObject> ()) {
	    return JS_UNDEFINED;
	}

	return container->adapter.instantiate (*candidate->as<WallpaperEngine::Scripting::ScriptableObject> ());
    }

    return JS_UNDEFINED;
}

JSValue scriptableobject_property_get (JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst receiver) {
    JSClassID classId = 0;

    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (obj_val, &classId));

    if (!container || container->magic != SCRIPTABLE_OPAQUE_MAGIC) {
	return JS_ThrowTypeError (ctx, "invalid receiver for property access");
    }

    const char* name = JS_AtomToCString (ctx, atom);

    if (name == nullptr) {
	return JS_EXCEPTION;
    }

    ScopeGuard guard ([=] { JS_FreeCString (ctx, name); });

    if (strcmp (name, "getTextureAnimation") == 0) {
	return JS_NewCFunction (ctx, scriptableobject_get_texture_animation, "getTextureAnimation", 0);
    }

    if (strcmp (name, "getChildren") == 0) {
	return JS_NewCFunction (ctx, scriptableobject_get_children, "getChildren", 0);
    }

    if (strcmp (name, "getParent") == 0) {
	return JS_NewCFunction (ctx, scriptableobject_get_parent, "getParent", 0);
    }

    // Read-only scene.json metadata — not backed by a scriptable DynamicValue like
    // origin/scale/angles/visible/etc. are, so getProperty() below would never find them.
    // Needed by scripts that filter thisScene.enumerateLayers()/getLayer() results by name or id.
    if (strcmp (name, "name") == 0) {
	return JS_NewString (ctx, container->object.getObject ().name.c_str ());
    }
    if (strcmp (name, "id") == 0) {
	return JS_NewInt32 (ctx, container->object.getId ());
    }

    try {
	// find the property inside, otherwise return undefined
	auto& property = container->object.getProperty (name);

	return container->adapter.getEngine ().dynamicToJs (property);
    } catch (const std::exception& e) {
	return JS_UNDEFINED;
    }
}

int scriptableobject_property_set (
    JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst val, JSValueConst receiver, int flags
) {
    JSClassID classId = 0;

    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (obj_val, &classId));

    if (!container || container->magic != SCRIPTABLE_OPAQUE_MAGIC) {
	JS_ThrowTypeError (ctx, "invalid receiver for property assignment");
	return -1;
    }

    const char* name = JS_AtomToCString (ctx, atom);

    if (name == nullptr) {
	return -1;
    }

    ScopeGuard nameGuard ([=] { JS_FreeCString (ctx, name); });

    try {
	auto& property = container->object.getProperty (name);
	container->adapter.getEngine ().jsToDynamic (val, property);
	return 1;
    } catch (const std::exception& e) {
	JS_ThrowTypeError (ctx, "%s", e.what ());
	return -1;
    }
}

ScriptableObjectAdapter::ScriptableObjectAdapter (ScriptEngine& engine, std::string name) :
    ObjectAdapter (engine),
    m_exoticMethods (
	{
	    .get_property = scriptableobject_property_get,
	    .set_property = scriptableobject_property_set,
	}
    ),
    m_name (std::move (name)) {
    this->registerType (
	{
	    .class_name = m_name.c_str (),
	    .exotic = &m_exoticMethods,
	}
    );
}

JSValue ScriptableObjectAdapter::instantiate (ScriptableObject& object) {
    JSValue result = this->ObjectAdapter::instantiate (object);
    JS_SetOpaque (
	result,
	new OpaqueScriptableObjectAdapter { .magic = SCRIPTABLE_OPAQUE_MAGIC, .adapter = *this, .object = object }
    );

    return result;
}

JSValue ScriptableObjectAdapter::instantiate (DynamicValue& value) {
    throw std::runtime_error ("Cannot create a ScriptableObject instance from a DynamicValue");
}

WallpaperEngine::Scripting::ScriptableObject* ScriptableObjectAdapter::extract (JSValueConst value) {
    JSClassID classId = 0;
    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (value, &classId));

    if (!container || container->magic != SCRIPTABLE_OPAQUE_MAGIC) {
	return nullptr;
    }

    return &container->object;
}