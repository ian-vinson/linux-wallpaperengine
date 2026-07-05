#include "ScriptableObjectAdapter.h"

#include <cstring>
#include <stdexcept>
#include <utility>

#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Render/Objects/CRenderable.h"
#include "WallpaperEngine/Render/Objects/CSound.h"
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

// no-op stand-in for ITextureAnimation.play()/stop()/pause() on layer types that don't render
// through CRenderable (CSound, CText) — see scriptableobject_get_texture_animation.
JSValue scriptableobject_texture_animation_noop (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    return JS_UNDEFINED;
}

// Recovers the CRenderable a getTextureAnimation() controller was created for. The controller
// functions are created with JS_NewCFunctionData() bound to the *original* thisLayer proxy
// JSValue (which carries the real OpaqueScriptableObjectAdapter) as func_data[0] — not the
// "anim" object itself, since a plain JS_NewObject() instance can't hold native opaque data
// (JS_SetOpaque only works on objects of a registered custom class; see
// ScriptableObjectAdapter::ScriptableObjectAdapter's registerType() call below).
WallpaperEngine::Render::Objects::CRenderable* textureAnimationRenderableFromBoundData (JSValueConst* funcData) {
    JSClassID classId = 0;
    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (funcData[0], &classId));

    if (!container || container->magic != SCRIPTABLE_OPAQUE_MAGIC
	|| !container->object.is<WallpaperEngine::Render::Objects::CRenderable> ()) {
	return nullptr;
    }

    return container->object.as<WallpaperEngine::Render::Objects::CRenderable> ();
}

JSValue texture_animation_get_rate (
    JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic, JSValueConst* funcData
) {
    auto* renderable = textureAnimationRenderableFromBoundData (funcData);

    if (renderable == nullptr) {
	return JS_ThrowTypeError (ctx, "getTextureAnimation().rate accessed on an invalid receiver");
    }

    return JS_NewFloat64 (ctx, renderable->getAnimationRate ());
}

JSValue texture_animation_set_rate (
    JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic, JSValueConst* funcData
) {
    auto* renderable = textureAnimationRenderableFromBoundData (funcData);

    if (renderable == nullptr) {
	return JS_ThrowTypeError (ctx, "getTextureAnimation().rate assigned on an invalid receiver");
    }

    double rate = 1.0;
    JS_ToFloat64 (ctx, &rate, argv[0]);
    renderable->setAnimationRate (static_cast<float> (rate));

    return JS_UNDEFINED;
}

JSValue texture_animation_play (
    JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic, JSValueConst* funcData
) {
    auto* renderable = textureAnimationRenderableFromBoundData (funcData);

    if (renderable == nullptr) {
	return JS_ThrowTypeError (ctx, "getTextureAnimation().play() called on an invalid receiver");
    }

    renderable->setAnimationPaused (false);
    return JS_UNDEFINED;
}

JSValue texture_animation_pause (
    JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic, JSValueConst* funcData
) {
    auto* renderable = textureAnimationRenderableFromBoundData (funcData);

    if (renderable == nullptr) {
	return JS_ThrowTypeError (ctx, "getTextureAnimation().pause() called on an invalid receiver");
    }

    renderable->setAnimationPaused (true);
    return JS_UNDEFINED;
}

JSValue texture_animation_stop (
    JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic, JSValueConst* funcData
) {
    auto* renderable = textureAnimationRenderableFromBoundData (funcData);

    if (renderable == nullptr) {
	return JS_ThrowTypeError (ctx, "getTextureAnimation().stop() called on an invalid receiver");
    }

    renderable->stopAnimation ();
    return JS_UNDEFINED;
}

JSValue texture_animation_set_frame (
    JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic, JSValueConst* funcData
) {
    auto* renderable = textureAnimationRenderableFromBoundData (funcData);

    if (renderable == nullptr) {
	return JS_ThrowTypeError (ctx, "getTextureAnimation().setFrame() called on an invalid receiver");
    }

    if (argc < 1) {
	return JS_ThrowTypeError (ctx, "setFrame() expects one argument");
    }

    int frame = 0;
    JS_ToInt32 (ctx, &frame, argv[0]);

    if (frame < 0) {
	return JS_ThrowTypeError (ctx, "setFrame() expects a non-negative frame index");
    }

    renderable->setAnimationFrame (static_cast<uint32_t> (frame));
    return JS_UNDEFINED;
}

// thisLayer.getTextureAnimation(): real Wallpaper Engine returns an ITextureAnimation controller
// ({rate, play(), stop(), pause(), setFrame()}) that drives sprite-sheet playback. Backed by real
// per-object state on CRenderable (m_animationElapsedTime/m_animationRate/m_animationPaused) —
// CPass::resolveTextureAnimationState() reads that state instead of the scene's global render
// clock for frame selection. Only CImage/CParticle inherit CRenderable; CSound/CText (any other
// ScriptableObject) fall back to the original inert no-op stub, since they have no texture
// animation state to control.
JSValue scriptableobject_get_texture_animation (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (this_val, &classId));
    const bool isRenderable
	= container && container->magic == SCRIPTABLE_OPAQUE_MAGIC
	&& container->object.is<WallpaperEngine::Render::Objects::CRenderable> ();

    JSValue anim = JS_NewObject (ctx);

    if (!isRenderable) {
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

    // JS_NewCFunctionData dups each bound JSValueConst internally, so passing this_val (borrowed
    // from the caller) here is safe — the closure keeps its own reference.
    JSValueConst boundData[] = { this_val };

    JS_DefinePropertyGetSet (
	ctx, anim, JS_NewAtom (ctx, "rate"),
	JS_NewCFunctionData (ctx, texture_animation_get_rate, 0, 0, 1, boundData),
	JS_NewCFunctionData (ctx, texture_animation_set_rate, 1, 0, 1, boundData), JS_PROP_ENUMERABLE
    );
    JS_SetPropertyStr (ctx, anim, "play", JS_NewCFunctionData (ctx, texture_animation_play, 0, 0, 1, boundData));
    JS_SetPropertyStr (ctx, anim, "pause", JS_NewCFunctionData (ctx, texture_animation_pause, 0, 0, 1, boundData));
    JS_SetPropertyStr (ctx, anim, "stop", JS_NewCFunctionData (ctx, texture_animation_stop, 0, 0, 1, boundData));
    JS_SetPropertyStr (
	ctx, anim, "setFrame", JS_NewCFunctionData (ctx, texture_animation_set_frame, 1, 0, 1, boundData)
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

// ISoundLayer's isPlaying()/play()/stop()/pause() (thisLayer on a sound-type layer). Only ever
// attached by scriptableobject_property_get when the underlying object is a CSound, so the
// is<CSound>() check here is a defensive backstop, not the primary gate.
JSValue scriptableobject_sound_is_playing (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (this_val, &classId));

    if (!container || container->magic != SCRIPTABLE_OPAQUE_MAGIC
	|| !container->object.is<WallpaperEngine::Render::Objects::CSound> ()) {
	return JS_ThrowTypeError (ctx, "isPlaying() called on an invalid receiver");
    }

    return JS_NewBool (ctx, container->object.as<WallpaperEngine::Render::Objects::CSound> ()->isPlaying ());
}

JSValue scriptableobject_sound_play (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (this_val, &classId));

    if (!container || container->magic != SCRIPTABLE_OPAQUE_MAGIC
	|| !container->object.is<WallpaperEngine::Render::Objects::CSound> ()) {
	return JS_ThrowTypeError (ctx, "play() called on an invalid receiver");
    }

    container->object.as<WallpaperEngine::Render::Objects::CSound> ()->play ();
    return JS_UNDEFINED;
}

JSValue scriptableobject_sound_stop (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (this_val, &classId));

    if (!container || container->magic != SCRIPTABLE_OPAQUE_MAGIC
	|| !container->object.is<WallpaperEngine::Render::Objects::CSound> ()) {
	return JS_ThrowTypeError (ctx, "stop() called on an invalid receiver");
    }

    container->object.as<WallpaperEngine::Render::Objects::CSound> ()->stop ();
    return JS_UNDEFINED;
}

JSValue scriptableobject_sound_pause (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (this_val, &classId));

    if (!container || container->magic != SCRIPTABLE_OPAQUE_MAGIC
	|| !container->object.is<WallpaperEngine::Render::Objects::CSound> ()) {
	return JS_ThrowTypeError (ctx, "pause() called on an invalid receiver");
    }

    container->object.as<WallpaperEngine::Render::Objects::CSound> ()->pause ();
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

    // ISoundLayer surface — only meaningful (and only exposed) on sound-type layers, so a
    // non-sound layer's `.isPlaying`/`.play`/`.stop`/`.pause` fall through to plain `undefined`
    // below, same as any other nonexistent property, rather than resolving to a function that
    // always throws when called.
    if (container->object.is<WallpaperEngine::Render::Objects::CSound> ()) {
	if (strcmp (name, "isPlaying") == 0) {
	    return JS_NewCFunction (ctx, scriptableobject_sound_is_playing, "isPlaying", 0);
	}
	if (strcmp (name, "play") == 0) {
	    return JS_NewCFunction (ctx, scriptableobject_sound_play, "play", 0);
	}
	if (strcmp (name, "stop") == 0) {
	    return JS_NewCFunction (ctx, scriptableobject_sound_stop, "stop", 0);
	}
	if (strcmp (name, "pause") == 0) {
	    return JS_NewCFunction (ctx, scriptableobject_sound_pause, "pause", 0);
	}
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