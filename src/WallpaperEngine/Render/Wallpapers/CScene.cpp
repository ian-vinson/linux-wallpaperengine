#include "WallpaperEngine/Render/Objects/CImage.h"
#include "WallpaperEngine/Render/Objects/CParticle.h"
#include "WallpaperEngine/Render/Objects/CSound.h"
#include "WallpaperEngine/Render/Objects/CText.h"

#include "WallpaperEngine/Render/WallpaperState.h"

#include "CScene.h"
#include "WallpaperEngine/Logging/Log.h"

#include "WallpaperEngine/Data/Model/Wallpaper.h"
#include "WallpaperEngine/Data/Parsers/ObjectParser.h"

#include <glm/glm.hpp>
#include <ranges>

#include "WallpaperEngine/Input/InputContext.h"
#include "WallpaperEngine/Input/MouseInput.h"

extern float g_Time;
extern float g_TimeLast;

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Render::Wallpapers;

CScene::CScene (
    const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext,
    const WallpaperState::TextureUVsScaling& scalingMode, const uint32_t& clampMode, const float& offsetX,
    const float& offsetY, const float& contrast, const float& saturation, const glm::vec3& borderColour
) :
    CWallpaper (
	wallpaper, context, audioContext, scalingMode, clampMode, offsetX, offsetY, contrast, saturation, borderColour
    ) {
    // caller should check this, if not a std::bad_cast is good to throw
    auto scene = wallpaper.as<Scene> ();

    // setup scripting engine
    this->m_scriptEngine = std::make_unique<Scripting::ScriptEngine> (*this, context.getMediaSource ());
    // setup the scene camera
    this->m_camera = std::make_unique<Camera> (*this, scene->camera);

    float width = scene->camera.projection.width;
    float height = scene->camera.projection.height;

    // detect size if the orthogonal project is auto
    if (scene->camera.projection.isAuto) {
	glm::vec2 maxExtent = { 0.0f, 0.0f };

	for (const auto& object : scene->objects) {
	    if (!object->is<Image> ()) {
		continue;
	    }

	    const auto* image = object->as<Image> ();
	    if (!image->origin || !image->origin->value) {
		continue;
	    }

	    const glm::vec3 origin = image->origin->value->getVec3 ();
	    const glm::vec2 halfSize = image->size / 2.0f;

	    maxExtent.x = glm::max (maxExtent.x, glm::abs (origin.x) + halfSize.x);
	    maxExtent.y = glm::max (maxExtent.y, glm::abs (origin.y) + halfSize.y);
	}

	if (maxExtent.x > 0.0f && maxExtent.y > 0.0f) {
	    width = maxExtent.x * 2.0f;
	    height = maxExtent.y * 2.0f;
	} else {
	    width = this->getContext ().getOutput ().getFullWidth ();
	    height = this->getContext ().getOutput ().getFullHeight ();
	    sLog.debug ("Auto projection: falling back to screen resolution ", width, "x", height);
	}
    }

    this->m_parallaxDisplacement = { 0, 0 };

    // TODO: CONVERSION
    if (scene->camera.projection.hasOrthogonal) {
        this->m_camera->setOrthogonalProjection (width, height);
        // Only fall back to "cover" scaling when the user didn't explicitly
        // request a --scaling mode (i.e. it's still at the untouched CLI
        // default). Some fallback is genuinely needed here -- DefaultUVs's
        // own generic aspect-mismatch handling caused black screens/distortion
        // for ortho scenes on non-native resolutions (commit 1a8b7a9, e.g.
        // #3448290956, Subaru on 3440x1440) -- but forcing it unconditionally
        // (the bug this replaces) silently overrode every explicit
        // --scaling request for any orthogonal-projection scene.
        if (this->getState ().getTextureUVsScaling () == WallpaperState::TextureUVsScaling::DefaultUVs) {
            this->setScalingMode (WallpaperState::TextureUVsScaling::ZoomFillUVs);
        }
    } else if (scene->camera.projection.perspectiveOverrideFov > 0.0f) {
        const float screenW = this->getContext ().getOutput ().getFullWidth ();
        const float screenH = this->getContext ().getOutput ().getFullHeight ();
        const float fov = scene->camera.projection.perspectiveOverrideFov;
        const float nearz = scene->camera.projection.nearz->value->getFloat ();
        const float farz = scene->camera.projection.farz->value->getFloat ();
        this->m_camera->setPerspectiveProjection (screenW, screenH, fov, nearz, farz);
    } else {
        const float screenW = this->getContext ().getOutput ().getFullWidth ();
        const float screenH = this->getContext ().getOutput ().getFullHeight ();
        this->m_camera->setOrthogonalProjection (screenW, screenH);
    }

    // setup framebuffers here as they're required for the scene setup
    this->setupFramebuffers ();

    const uint32_t sceneWidth = this->m_camera->getWidth ();
    const uint32_t sceneHeight = this->m_camera->getHeight ();

    this->_rt_shadowAtlas = this->create (
	"_rt_shadowAtlas", TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0, { sceneWidth, sceneHeight },
	{ sceneWidth, sceneHeight }
    );
    this->alias ("_alias_lightCookie", "_rt_shadowAtlas");

    // set clear color
    const glm::vec3 clearVec = scene->colors.clear->value->getVec3 ();

    glClearColor (clearVec.r, clearVec.g, clearVec.b, 1.0f);

    // create all objects based off their dependencies
    for (const auto& object : scene->objects) {
	this->createObject (*object);
    }

    // copy over objects by render order
    for (const auto& object : scene->objects) {
	this->addObjectToRenderOrder (*object);
    }

    // Every object now exists AND is registered in render order — safe to run their scripts'
    // deferred init()/update() priming calls. Must happen after both loops above, not per-object
    // inside either: scripts commonly resolve other layers by name via
    // thisScene.getLayer("someName") in init(), and that lookup searches
    // getObjectsByRenderOrder() (SceneObject.cpp's get_layer), not the construction-order object
    // map — so this has to wait for the render-order loop too, not just construction.
    this->m_scriptEngine->runPendingInits ();

    // create extra framebuffers for the bloom effect
    this->_rt_4FrameBuffer = this->create (
	"_rt_4FrameBuffer", TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0, { sceneWidth / 4, sceneHeight / 4 },
	{ sceneWidth / 4, sceneHeight / 4 }
    );
    this->_rt_8FrameBuffer = this->create (
	"_rt_8FrameBuffer", TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0, { sceneWidth / 8, sceneHeight / 8 },
	{ sceneWidth / 8, sceneHeight / 8 }
    );
    this->_rt_Bloom = this->create (
	"_rt_Bloom", TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0, { sceneWidth / 8, sceneHeight / 8 },
	{ sceneWidth / 8, sceneHeight / 8 }
    );

    //
    // Had to get a little creative with the effects to achieve the same bloom effect without any custom code
    // this custom image loads some effect files from the virtual container to achieve the same bloom effect
    // this approach requires of two extra draw calls due to the way the effect works in official WPE
    // (it renders directly to the screen, whereas here we never do that from a scene)
    //

    const auto bloomOrigin = glm::vec3 { sceneWidth / 2, sceneHeight / 2, 0.0f };
    const auto bloomSize = glm::vec2 { sceneWidth, sceneHeight };

    const JSON bloom
	= { { "image", "models/wpenginelinux.json" },
	    { "name", "bloomimagewpenginelinux" },
	    { "visible", true },
	    { "scale", "1.0 1.0 1.0" },
	    { "angles", "0.0 0.0 0.0" },
	    { "origin",
	      std::to_string (bloomOrigin.x) + " " + std::to_string (bloomOrigin.y) + " "
		  + std::to_string (bloomOrigin.z) },
	    { "size", std::to_string (bloomSize.x) + " " + std::to_string (bloomSize.y) },
	    { "id", -1 },
	    { "effects",
	      JSON::array (
		  { { { "file", "effects/wpenginelinux/bloomeffect.json" },
		      { "id", 15242000 },
		      { "name", "" },
		      { "passes",
			JSON::array (
			    { { { "constantshadervalues",
				  { { "bloomstrength", this->getScene ().camera.bloom.strength->value->getFloat () },
				    { "bloomthreshold",
				      this->getScene ().camera.bloom.threshold->value->getFloat () } } } },
			      { { "constantshadervalues",
				  { { "bloomstrength", this->getScene ().camera.bloom.strength->value->getFloat () },
				    { "bloomthreshold",
				      this->getScene ().camera.bloom.threshold->value->getFloat () } } } },
			      { { "constantshadervalues",
				  { { "bloomstrength", this->getScene ().camera.bloom.strength->value->getFloat () },
				    { "bloomthreshold",
				      this->getScene ().camera.bloom.threshold->value->getFloat () } } } } }
			) } } }
	      ) } };

    // create image for bloom passes
    if (scene->camera.bloom.enabled->value->getBool ()) {
	this->m_bloomObjectData = ObjectParser::parse (bloom, scene->project);
	this->m_bloomObject = this->createObject (*this->m_bloomObjectData);

	this->m_objectsByRenderOrder.push_back (this->m_bloomObject);
    }
}

CScene::~CScene () {
    // bloom object is in the objects list, so no need to explicitly delete it
    this->m_bloomObject = nullptr;

    for (const auto& val : this->m_objects | std::views::values) {
	delete val;
    }

    this->m_objectsByRenderOrder.clear ();
    this->m_objects.clear ();
}

Render::CObject* CScene::createObject (const Object& object) {
    Render::CObject* renderObject = nullptr;

    // ensure the item is not loaded already
    if (const auto current = this->m_objects.find (object.id); current != this->m_objects.end ()) {
	return current->second;
    }

    // check dependencies too!
    for (const auto& cur : object.dependencies) {
	// self-dependency is a possibility...
	if (cur == object.id) {
	    continue;
	}

	const auto dep
	    = std::ranges::find_if (this->getScene ().objects, [&cur] (const auto& o) { return o->id == cur; });

	if (dep != this->getScene ().objects.end ()) {
	    this->createObject (**dep);
	}
    }

    // check if the item has any parent and also create it first
    if (object.parent.has_value ()) {
	int parentId = object.parent.value ();

	const auto dep = std::ranges::find_if (this->getScene ().objects, [&parentId] (const auto& o) {
	    return o->id == parentId;
	});

	if (dep == this->getScene ().objects.end ()) {
	    sLog.exception ("Cannot find parent ", parentId, " for object ", object.id);
	}

	this->createObject (**dep);
    }

    renderObject = this->dispatchObjectType (object);

    if (renderObject != nullptr) {
	this->m_objects.emplace (renderObject->getId (), renderObject);
    }

    return renderObject;
}

Render::CObject* CScene::dispatchObjectType (const Object& object) {
    Render::CObject* renderObject = nullptr;

    if (object.is<Image> ()) {
	renderObject = new Objects::CImage (*this, *object.as<Image> ());
    } else if (object.is<Sound> ()) {
	renderObject = new Objects::CSound (*this, *object.as<Sound> ());
    } else if (object.is<Text> ()) {
	renderObject = new Objects::CText (*this, *object.as<Text> ());
    } else if (object.is<Particle> ()) {
	const auto& particleData = *object.as<Particle> ();

	if (this->getContext ().getApp ().getContext ().settings.general.disableParticles == true) {
	    sLog.debug ("Ignoring particle system (disabled in settings): ", particleData.name);
	    return nullptr;
	}

	// CParticle's constructor unconditionally dereferences *particleData.material->material in
	// its member-initializer list (to build CRenderable's base), which can't contain a null
	// check of its own. A particle definition with a missing/malformed "material" reference
	// (e.g. MaterialParser::load() throwing, caught upstream in ObjectParser::parseParticle()
	// leaving this null) must be rejected here, before ever reaching that constructor.
	if (particleData.material == nullptr || particleData.material->material == nullptr) {
	    sLog.error ("Particle system has no valid material, skipping: ", particleData.name);
	    return nullptr;
	}

	renderObject = new Objects::CParticle (*this, particleData);
    } else if (object.is<CameraObject> ()) {
	const auto& cam = *object.as<CameraObject> ();
	if (cam.cameraName == "default" && cam.origin && cam.origin->value) {
	    float zoom = (cam.zoom && cam.zoom->value) ? cam.zoom->value->getFloat () : 1.0f;
	    if (!getenv ("LWE_DISABLE_CAMERA_OBJECT")) {
		this->m_camera->applyObjectCamera (cam.origin->value->getVec3 (), zoom);
	    }
	}
	return nullptr;
    } else {
	sLog.debug ("Unknown object type, creating placeholder, empty object: ", object.id);
	renderObject = new CObject (*this, object);
    }

    try {
	renderObject->setup ();
    } catch (const std::exception& e) {
	sLog.error ("Failed to setup object ", object.id, ": ", e.what ());
	delete renderObject;
	renderObject = nullptr;
    }

    return renderObject;
}

void CScene::addObjectToRenderOrder (const Object& object) {
    const auto obj = this->m_objects.find (object.id);

    // ignores not created objects like particle systems
    if (obj == this->m_objects.end ()) {
	return;
    }

    // take into account any dependency first
    for (const auto& dep : object.dependencies) {
	// self-dependency is possible
	if (dep == object.id) {
	    continue;
	}

	// add the dependency to the list if it's created
	auto depIt = std::ranges::find_if (this->getScene ().objects, [&dep] (const auto& o) { return o->id == dep; });

	if (depIt != this->getScene ().objects.end ()) {
	    this->addObjectToRenderOrder (**depIt);
	} else {
	    sLog.error ("Cannot find dependency ", dep, " for object ", object.id);
	}
    }

    // ensure we're added only once to the render list
    const auto renderIt = std::ranges::find_if (this->m_objectsByRenderOrder, [&object] (const auto& o) {
	return o->getId () == object.id;
    });

    if (renderIt == this->m_objectsByRenderOrder.end ()) {
	this->m_objectsByRenderOrder.emplace_back (obj->second);
    }
}

Render::CObject* CScene::createLayer (const std::string& model) {
    const int id = this->m_nextDynamicObjectId--;

    const JSON definition = {
	{ "id", id },
	{ "name", "dynamic_layer_" + std::to_string (-id) },
	{ "image", model },
	{ "visible", true },
    };

    ObjectUniquePtr objectData;

    try {
	objectData = ObjectParser::parse (definition, this->getScene ().project);
    } catch (const std::exception& e) {
	sLog.error ("CScene::createLayer: failed to parse model '", model, "': ", e.what ());
	return nullptr;
    }

    if (objectData == nullptr) {
	return nullptr;
    }

    Render::CObject* object = this->createObject (*objectData);

    if (object == nullptr) {
	return nullptr;
    }

    this->m_dynamicObjectsData.push_back (std::move (objectData));
    this->m_objectsByRenderOrder.push_back (object);

    return object;
}

int CScene::getRenderOrderIndex (const CObject& object) const {
    const auto it = std::ranges::find (this->m_objectsByRenderOrder, &object);

    if (it == this->m_objectsByRenderOrder.end ()) {
	return -1;
    }

    return static_cast<int> (std::distance (this->m_objectsByRenderOrder.begin (), it));
}

void CScene::setRenderOrderIndex (CObject& object, int index) {
    const auto it = std::ranges::find (this->m_objectsByRenderOrder, &object);

    if (it == this->m_objectsByRenderOrder.end ()) {
	return;
    }

    this->m_objectsByRenderOrder.erase (it);

    index = std::clamp (index, 0, static_cast<int> (this->m_objectsByRenderOrder.size ()));
    this->m_objectsByRenderOrder.insert (this->m_objectsByRenderOrder.begin () + index, &object);
}

bool CScene::destroyObject (int id) {
    if (this->m_pendingDestruction.contains (id)) {
	return true; // already marked, dedupe repeat calls in the same frame
    }

    if (!this->m_objects.contains (id)) {
	return false;
    }

    this->m_pendingDestruction.insert (id);
    return true;
}

void CScene::processPendingDestructions () {
    if (this->m_pendingDestruction.empty ()) {
	return;
    }

    // Snapshot-and-clear rather than iterate m_pendingDestruction directly: nothing currently
    // marks further objects for destruction from within this loop, but this keeps the same
    // "drain what's here, don't get confused by concurrent mutation" shape as
    // ScriptEngine::runPendingInits()'s own snapshot-and-clear, for the same reason (defensive
    // against a future destructor path that itself calls destroyObject() on something else).
    const auto ids = std::move (this->m_pendingDestruction);
    this->m_pendingDestruction.clear ();

    for (const int id : ids) {
	const auto it = this->m_objects.find (id);

	if (it == this->m_objects.end ()) {
	    continue; // already gone somehow — nothing to do
	}

	CObject* object = it->second;

	// (b) Forget this object's property-scripts BEFORE deleting it — tick() dereferences
	// every m_scriptModules entry's object reference unconditionally, every frame, so a
	// stale entry left behind would crash/UB on the very next tick(), independent of any
	// JS-side reference to the object.
	if (object->is<Scripting::ScriptableObject> ()) {
	    this->getScriptEngine ().forgetObject (*object->as<Scripting::ScriptableObject> ());
	}

	// (c) Remove from both containers that let anything else find this object by id or by
	// render-order scan (getLayer/getLayerByID/enumerateLayers/getChildren/getParent/
	// getObject all go through one of these two).
	this->m_objects.erase (it);

	if (const auto renderIt = std::ranges::find (this->m_objectsByRenderOrder, object);
	    renderIt != this->m_objectsByRenderOrder.end ()) {
	    this->m_objectsByRenderOrder.erase (renderIt);
	}

	// (d) Drop this scene's own FBOProvider references to any scene-level FBOs this object
	// registered — currently only CImage does this (its own main/sub composite FBOs,
	// registered via scene.create() in its constructor as "_rt_imageLayerComposite_<id>_a"/
	// "_b"). Without this, this scene's own FBOProvider::m_fbos map would keep an
	// independent shared_ptr<CFBO> alive forever, leaking the GPU texture even after the
	// CImage itself is deleted below.
	if (object->is<Objects::CImage> ()) {
	    const std::string base = "_rt_imageLayerComposite_" + std::to_string (object->getId ());
	    this->remove (base + "_a");
	    this->remove (base + "_b");
	}

	// (e) Finally, delete the object itself. Its own destructor cascade already does the
	// right thing (CText's script handle via ScriptEngine::destroyLayer(), CSound's
	// AudioStreams via stop(), CImage's VBOs/passes/own FBO shared_ptr members, GL cleanup)
	// — no changes needed there.
	delete object;
    }
}

ScriptEngine& CScene::getScriptEngine () const { return *this->m_scriptEngine; }
Camera& CScene::getCamera () const { return *this->m_camera; }

glm::vec3 CScene::getCursorWorldPosition () const {
    const glm::mat4 mvp = this->m_camera->getProjection () * this->m_camera->getLookAt ();
    const glm::mat4 inverseMvp = glm::inverse (mvp);

    // m_mousePositionNormalized is already 0..1 in OpenGL convention (0=bottom/left, 1=top/right),
    // matching the GL-centered, Y-up space the projection/lookAt matrices operate in.
    glm::vec4 clip (
	this->m_mousePositionNormalized.x * 2.0f - 1.0f, this->m_mousePositionNormalized.y * 2.0f - 1.0f, 0.0f, 1.0f
    );

    glm::vec4 world = inverseMvp * clip;

    if (world.w != 0.0f) {
	world /= world.w;
    }

    const float width = this->m_camera->getWidth ();
    const float height = this->m_camera->getHeight ();

    // GL-centered (Y-up) -> WE-space (top-left origin, Y-down), same convention as
    // Camera::applyObjectCamera and CImage::updateScenePosition.
    return { world.x + width / 2.0f, height / 2.0f - world.y, 0.0f };
}

glm::vec2 CScene::getCursorScreenPosition () const {
    return { this->m_mousePosition.x * this->m_camera->getWidth (), this->m_mousePosition.y * this->m_camera->getHeight () };
}

bool CScene::isCursorLeftDown () const {
    return this->getContext ().getInputContext ().getMouseInput ().leftClick () == Input::MouseClickStatus::Clicked;
}

void CScene::renderFrame (const glm::ivec4& viewport) {
    // ensure the virtual mouse position is up to date
    this->updateMouse (viewport);

    // update the parallax position if required
    if (this->getScene ().camera.parallax.enabled->value->getBool ()
	&& !this->getContext ().getApp ().getContext ().settings.mouse.disableparallax) {
	const float influence = this->getScene ().camera.parallax.mouseInfluence->value->getFloat ();
	const float amount = this->getScene ().camera.parallax.amount->value->getFloat ();
	const float delay = glm::clamp (
	    this->getScene ().camera.parallax.delay->value->getFloat () * (g_Time - g_TimeLast), 0.0f, 1.0f
	);

	const glm::vec2 centeredMouse = this->m_mousePosition - glm::vec2 (0.5f, 0.5f);
	this->m_parallaxDisplacement
	    = glm::mix (this->m_parallaxDisplacement, (centeredMouse * amount) * influence, delay);
    }

    // run a tick in the javascript logic
    this->getScriptEngine ().tick ();

    // Actually tear down anything thisScene.destroyLayer() marked during the tick just above —
    // deliberately after tick() (every script this frame, including the one that called
    // destroyLayer() on itself or another object, has already run against fully-alive state) and
    // before anything below that iterates m_objectsByRenderOrder.
    this->processPendingDestructions ();

    // update main textures for images
    for (const auto& cur : this->m_objectsByRenderOrder) {
	if (!cur->is<Objects::CImage> ()) {
	    continue;
	}

	const Objects::CImage* image = cur->as<Objects::CImage> ();

#if !NDEBUG
	const std::string message = "Updating texture " + image->getImage ().model->filename;

	glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, message.c_str ());
#endif

	image->getTexture ()->update ();

#if !NDEBUG
	glPopDebugGroup ();
#endif
    }

    // Video-backed textures referenced only as effect/material inputs (e.g. day/night blend
    // textures) aren't anyone's "main" texture, so the per-object loop above never reaches them —
    // update() every cached texture too so they actually decode. Cheap no-op for anything that
    // isn't video-backed (CFBO/AlbumTexture's update() are empty), and harmless to call twice on
    // an object's own main texture (already updated above) since it just re-renders the current
    // frame.
    this->getContext ().updateAllTextures ();

    // bind the vertex array
    glBindVertexArray (this->m_vaoBuffer);
    // use the scene's framebuffer by default
    glBindFramebuffer (GL_FRAMEBUFFER, this->getWallpaperFramebuffer ());
    // ensure we render over the whole framebuffer
    glViewport (0, 0, this->m_sceneFBO->getRealWidth (), this->m_sceneFBO->getRealHeight ());

    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (const auto& cur : this->m_objectsByRenderOrder) {
	const auto& debug = this->getContext ().getApp ().getContext ().settings.render.debug;
	if (debug.objectFilter.has_value () && cur->getId () != debug.objectFilter.value ()) {
	    continue;
	}
	if (std::ranges::find (debug.skipObjects, cur->getId ()) != debug.skipObjects.end ()) {
	    continue;
	}

	cur->render ();
    }
}

void CScene::updateMouse (const glm::ivec4& viewport) {
    // update virtual mouse position first
    const glm::dvec2 position = this->getContext ().getInputContext ().getMouseInput ().position ();

    // rollover the position to the last
    this->m_mousePositionLast = this->m_mousePosition;

    // calculate the current position of the mouse in viewport space [0, 1]
    double mouseX = glm::clamp ((position.x - viewport.x) / viewport.z, 0.0, 1.0);
    // Normalize Y coordinate (OpenGL convention: 0=bottom, 1=top)
    // Particle code expects this convention: 0=bottom results in negative Y (down), 1=top results in positive Y (up)
    double normalizedMouseY = glm::clamp ((position.y - viewport.y) / viewport.w, 0.0, 1.0);

    // Account for UV cropping when using fill/fit scaling modes
    // The scene may be rendered larger than viewport and cropped via UVs
    const auto uvs = this->getState ().getTextureUVs ();

    // Map mouse position from viewport space to scene UV space
    // UVs define what portion of the scene texture is visible
    this->m_mousePositionNormalized.x = uvs.ustart + mouseX * (uvs.uend - uvs.ustart);
    this->m_mousePositionNormalized.y = uvs.vstart + normalizedMouseY * (uvs.vend - uvs.vstart);

    // Invert previous normalization of Y to match what the shader expects
    double mouseY = 1.0 - normalizedMouseY;

    this->m_mousePosition.x = this->m_mousePositionNormalized.x;
    this->m_mousePosition.y = uvs.vstart + mouseY * (uvs.vend - uvs.vstart);
}

const Scene& CScene::getScene () const { return *this->getWallpaperData ().as<Scene> (); }

int CScene::getWidth () const { return this->m_camera->getWidth (); }

int CScene::getHeight () const { return this->m_camera->getHeight (); }

float CScene::getTime () const { return g_Time; }

float CScene::getDeltaTime () const { return g_Time - g_TimeLast; }

float CScene::getFps () const {
    const float dt = g_Time - g_TimeLast;
    // Guard against the first frame (where g_TimeLast is 0 so dt == g_Time)
    // and division by zero on the very first call.
    if (dt <= 1e-6f) {
	return 60.0f;
    }
    return 1.0f / dt;
}

const glm::vec2* CScene::getMousePosition () const { return &this->m_mousePosition; }

const glm::vec2* CScene::getMousePositionLast () const { return &this->m_mousePositionLast; }

const glm::vec2* CScene::getMousePositionNormalized () const { return &this->m_mousePositionNormalized; }

const glm::vec2* CScene::getParallaxDisplacement () const { return &this->m_parallaxDisplacement; }

const std::vector<CObject*>& CScene::getObjectsByRenderOrder () const { return this->m_objectsByRenderOrder; }

const CObject* CScene::getObject (int id) const {
    const auto object = this->m_objects.find (id);
    return object == this->m_objects.end () ? nullptr : object->second;
}
