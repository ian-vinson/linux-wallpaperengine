#include "ScriptableObjectAdapter.h"

#include <cstring>
#include <stdexcept>
#include <utility>

#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Render/Objects/CParticle.h"
#include "WallpaperEngine/Render/Objects/CRenderable.h"
#include "WallpaperEngine/Render/Objects/CSound.h"
#include "WallpaperEngine/Scripting/Adapters/Mat4Adapter.h"
#include "WallpaperEngine/Scripting/ScriptEngine.h"
#include "WallpaperEngine/Scripting/ScriptableObject.h"

using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::Data::Utils;
using namespace WallpaperEngine::Scripting::Adapters;

#define SCRIPTABLE_OPAQUE_MAGIC 0xdeadbeef

// Deliberately does NOT hold a direct reference to the underlying ScriptableObject — objects can
// now be destroyed mid-session (thisScene.destroyLayer()), while a JS-side reference to this
// wrapper can outlive that (a script capturing `let saved = thisLayer;` in module-level state).
// Holding `scene` + `objectId` instead means every access re-resolves fresh via
// CScene::getObject(), which safely returns nullptr once the object is gone — no dangling
// reference is ever dereferenced. `scene` itself only dies at full wallpaper teardown, matching
// the JS runtime's own lifetime, so this reference is always safe to hold long-term.
struct OpaqueScriptableObjectAdapter {
    unsigned int magic;
    ScriptableObjectAdapter& adapter;
    WallpaperEngine::Render::Wallpapers::CScene& scene;
    int objectId;
};

// Resolves the opaque wrapper back to its underlying ScriptableObject, or nullptr if the
// container is invalid OR the underlying object has since been destroyed
// (thisScene.destroyLayer()). Every exotic dispatch function in this file must go through this
// rather than caching/dereferencing anything from a previous call — see the struct comment above.
WallpaperEngine::Scripting::ScriptableObject* resolveScriptableObject (OpaqueScriptableObjectAdapter* container) {
    if (!container || container->magic != SCRIPTABLE_OPAQUE_MAGIC) {
	return nullptr;
    }

    const auto* object = container->scene.getObject (container->objectId);

    if (object == nullptr || !object->is<WallpaperEngine::Scripting::ScriptableObject> ()) {
	return nullptr;
    }

    return const_cast<WallpaperEngine::Scripting::ScriptableObject*> (
	object->as<WallpaperEngine::Scripting::ScriptableObject> ()
    );
}

void scriptableobject_finalizer (JSRuntime* rt, JSValueConst val) {
    JSClassID classId = 0;
    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (val, &classId));

    if (!container || container->magic != SCRIPTABLE_OPAQUE_MAGIC) {
	return;
    }

    // Only ever frees the small wrapper struct itself — it never owned the underlying
    // ScriptableObject (that's owned by CScene), so there's nothing else to clean up here.
    delete container;
}

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
    auto* object = resolveScriptableObject (container);

    if (object == nullptr || !object->is<WallpaperEngine::Render::Objects::CRenderable> ()) {
	return nullptr;
    }

    return object->as<WallpaperEngine::Render::Objects::CRenderable> ();
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
    auto* object = resolveScriptableObject (container);
    const bool isRenderable = object != nullptr && object->is<WallpaperEngine::Render::Objects::CRenderable> ();

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
    auto* self = resolveScriptableObject (container);

    if (self == nullptr) {
	return JS_ThrowTypeError (ctx, "getChildren() called on an invalid receiver");
    }

    JSValue array = JS_NewArray (ctx);
    uint32_t index = 0;
    const int parentId = self->getId ();

    for (auto* candidate : container->scene.getObjectsByRenderOrder ()) {
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
    auto* self = resolveScriptableObject (container);

    if (self == nullptr) {
	return JS_ThrowTypeError (ctx, "getParent() called on an invalid receiver");
    }

    const auto& parent = self->getObject ().parent;

    if (!parent.has_value ()) {
	return JS_UNDEFINED;
    }

    const int parentId = parent.value ();

    for (auto* candidate : container->scene.getObjectsByRenderOrder ()) {
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
    auto* object = resolveScriptableObject (container);

    if (object == nullptr || !object->is<WallpaperEngine::Render::Objects::CSound> ()) {
	return JS_ThrowTypeError (ctx, "isPlaying() called on an invalid receiver");
    }

    return JS_NewBool (ctx, object->as<WallpaperEngine::Render::Objects::CSound> ()->isPlaying ());
}

JSValue scriptableobject_sound_play (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (this_val, &classId));
    auto* object = resolveScriptableObject (container);

    if (object == nullptr || !object->is<WallpaperEngine::Render::Objects::CSound> ()) {
	return JS_ThrowTypeError (ctx, "play() called on an invalid receiver");
    }

    object->as<WallpaperEngine::Render::Objects::CSound> ()->play ();
    return JS_UNDEFINED;
}

JSValue scriptableobject_sound_stop (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (this_val, &classId));
    auto* object = resolveScriptableObject (container);

    if (object == nullptr || !object->is<WallpaperEngine::Render::Objects::CSound> ()) {
	return JS_ThrowTypeError (ctx, "stop() called on an invalid receiver");
    }

    object->as<WallpaperEngine::Render::Objects::CSound> ()->stop ();
    return JS_UNDEFINED;
}

JSValue scriptableobject_sound_pause (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (this_val, &classId));
    auto* object = resolveScriptableObject (container);

    if (object == nullptr || !object->is<WallpaperEngine::Render::Objects::CSound> ()) {
	return JS_ThrowTypeError (ctx, "pause() called on an invalid receiver");
    }

    object->as<WallpaperEngine::Render::Objects::CSound> ()->pause ();
    return JS_UNDEFINED;
}

// thisLayer.getTransformMatrix(): real Wallpaper Engine returns the object's world-space
// transform. Only CParticle has a genuine, well-formed TRS matrix available (m_modelMatrix) —
// CImage/CText's rotation tracking silently drops any X/Y angle components (a separate,
// independent correctness gap out of scope here), so they honestly return undefined rather than
// a plausible-looking but incomplete result. CParticle's own m_modelMatrix never walks the
// parent chain either (confirmed: its origin comes directly from Particle::origin, no parent
// lookup), so it's only genuinely a WORLD matrix when the object is unparented — a parented
// particle also returns undefined rather than silently handing back a local-only matrix under
// the documented "world transform" name.
JSValue scriptableobject_get_transform_matrix (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (this_val, &classId));
    auto* object = resolveScriptableObject (container);

    if (object == nullptr || !object->is<WallpaperEngine::Render::Objects::CParticle> ()) {
        return JS_UNDEFINED;
    }

    if (object->getObject ().parent.has_value ()) {
        return JS_UNDEFINED;
    }

    const auto* particle = object->as<WallpaperEngine::Render::Objects::CParticle> ();

    return container->adapter.getEngine ().getAdapters ().mat4->wrap (particle->getModelMatrix ());
}

// thisLayer.getAttachmentMatrix()/getAttachmentOrigin()/getAttachmentAngles(): real Wallpaper
// Engine's puppet-warp/bone attachment API. This fork has zero attachment/puppet/bone
// infrastructure (confirmed by repo-wide search), so these can't be implemented — but they're
// still exposed as real, callable functions (matching real WE's API surface, so
// `typeof thisLayer.getAttachmentMatrix === 'function'` succeeds) that throw a clear, specific
// error when actually called, rather than being silently missing or returning undefined for a
// script to fail on confusingly later (e.g. `undefined.transformPoint(...)`).
JSValue scriptableobject_get_attachment_matrix (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    return JS_ThrowTypeError (ctx, "getAttachmentMatrix() is not supported: this build has no puppet-warp/attachment system");
}

JSValue scriptableobject_get_attachment_origin (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    return JS_ThrowTypeError (ctx, "getAttachmentOrigin() is not supported: this build has no puppet-warp/attachment system");
}

JSValue scriptableobject_get_attachment_angles (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    return JS_ThrowTypeError (ctx, "getAttachmentAngles() is not supported: this build has no puppet-warp/attachment system");
}

JSValue scriptableobject_property_get (JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst receiver) {
    JSClassID classId = 0;

    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (obj_val, &classId));
    auto* self = resolveScriptableObject (container);

    // The underlying object may have been destroyed (thisScene.destroyLayer()) since this JS
    // wrapper was handed to a script — treat it exactly like any other missing property (below)
    // rather than throwing, so `if (savedLayer.someProp === undefined)`-style guards keep working.
    if (self == nullptr) {
	return JS_UNDEFINED;
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

    if (strcmp (name, "getTransformMatrix") == 0) {
	return JS_NewCFunction (ctx, scriptableobject_get_transform_matrix, "getTransformMatrix", 0);
    }

    if (strcmp (name, "getAttachmentMatrix") == 0) {
	return JS_NewCFunction (ctx, scriptableobject_get_attachment_matrix, "getAttachmentMatrix", 1);
    }

    if (strcmp (name, "getAttachmentOrigin") == 0) {
	return JS_NewCFunction (ctx, scriptableobject_get_attachment_origin, "getAttachmentOrigin", 1);
    }

    if (strcmp (name, "getAttachmentAngles") == 0) {
	return JS_NewCFunction (ctx, scriptableobject_get_attachment_angles, "getAttachmentAngles", 1);
    }

    // ISoundLayer surface — only meaningful (and only exposed) on sound-type layers, so a
    // non-sound layer's `.isPlaying`/`.play`/`.stop`/`.pause` fall through to plain `undefined`
    // below, same as any other nonexistent property, rather than resolving to a function that
    // always throws when called.
    if (self->is<WallpaperEngine::Render::Objects::CSound> ()) {
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
	return JS_NewString (ctx, self->getObject ().name.c_str ());
    }
    if (strcmp (name, "id") == 0) {
	return JS_NewInt32 (ctx, self->getId ());
    }

    try {
	// find the property inside, otherwise return undefined
	auto& property = self->getProperty (name);

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
    auto* self = resolveScriptableObject (container);

    if (self == nullptr) {
	JS_ThrowTypeError (ctx, "invalid receiver for property assignment");
	return -1;
    }

    const char* name = JS_AtomToCString (ctx, atom);

    if (name == nullptr) {
	return -1;
    }

    ScopeGuard nameGuard ([=] { JS_FreeCString (ctx, name); });

    try {
	auto& property = self->getProperty (name);
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
	    .finalizer = scriptableobject_finalizer,
	    .exotic = &m_exoticMethods,
	}
    );
}

JSValue ScriptableObjectAdapter::instantiate (ScriptableObject& object) {
    JSValue result = this->ObjectAdapter::instantiate (object);
    JS_SetOpaque (
	result,
	new OpaqueScriptableObjectAdapter {
	    .magic = SCRIPTABLE_OPAQUE_MAGIC, .adapter = *this, .scene = object.getScene (),
	    .objectId = object.getId ()
	}
    );

    return result;
}

JSValue ScriptableObjectAdapter::instantiate (DynamicValue& value) {
    throw std::runtime_error ("Cannot create a ScriptableObject instance from a DynamicValue");
}

WallpaperEngine::Scripting::ScriptableObject* ScriptableObjectAdapter::extract (JSValueConst value) {
    JSClassID classId = 0;
    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (value, &classId));

    return resolveScriptableObject (container);
}