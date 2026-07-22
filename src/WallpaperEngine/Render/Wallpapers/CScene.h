#pragma once

#include <unordered_set>

#include "WallpaperEngine/Render/Camera.h"

#include "WallpaperEngine/Render/CWallpaper.h"
#include "WallpaperEngine/Scripting/ScriptEngine.h"

namespace WallpaperEngine::Render {
class Camera;
class CObject;
}

namespace WallpaperEngine::Render::Wallpapers {
using namespace WallpaperEngine::Data::Model;

class CScene final : public CWallpaper {
public:
    CScene (
	const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext,
	const WallpaperState::TextureUVsScaling& scalingMode, const uint32_t& clampMode, const float& offsetX = 0.0f,
	const float& offsetY = 0.0f, const float& contrast = 1.0f, const float& saturation = 1.0f,
	const glm::vec3& borderColour = { 0.0f, 0.0f, 0.0f }
    );

    ~CScene () override;

    [[nodiscard]] Scripting::ScriptEngine& getScriptEngine () const;
    [[nodiscard]] Camera& getCamera () const;

    [[nodiscard]] const Scene& getScene () const;

    [[nodiscard]] int getWidth () const override;
    [[nodiscard]] int getHeight () const override;

    // Time accessors used by dynamic text layers (CText + ScriptEngine).
    // Read from the application-wide g_Time/g_TimeLast globals that other
    // renderers already consume via extern (e.g. CParticle).
    [[nodiscard]] float getTime () const;
    [[nodiscard]] float getDeltaTime () const;
    [[nodiscard]] float getFps () const;

    const glm::vec2* getMousePosition () const;
    const glm::vec2* getMousePositionLast () const;
    const glm::vec2* getMousePositionNormalized () const;
    const glm::vec2* getParallaxDisplacement () const;

    /** Cursor position in WE world-space (top-left origin, Y-down), accounting for any active camera zoom/pan. */
    [[nodiscard]] glm::vec3 getCursorWorldPosition () const;
    /** Cursor position in WE scene-pixel space (top-left origin, Y-down), ignoring camera zoom/pan. */
    [[nodiscard]] glm::vec2 getCursorScreenPosition () const;
    [[nodiscard]] bool isCursorLeftDown () const;

    [[nodiscard]] const std::vector<CObject*>& getObjectsByRenderOrder () const;
    [[nodiscard]] const CObject* getObject (int id) const;

    /**
     * Instantiates a new renderable layer at runtime from a model json path (e.g.
     * "models/workshop/2079954552/bar.json"), appends it to the render order and returns it.
     * Returns nullptr if the model/object could not be constructed.
     */
    Render::CObject* createLayer (const std::string& model);

    /** @return the layer's current index in render order, or -1 if it isn't part of this scene. */
    [[nodiscard]] int getRenderOrderIndex (const CObject& object) const;

    /** Moves the layer to the given index in render order, clamping to the valid range. */
    void setRenderOrderIndex (CObject& object, int index);

    /**
     * Marks the given object id for deferred destruction — matches real Wallpaper Engine's own
     * documented removeLayer() semantics ("the layer will actually be removed in a deferred
     * manner"). The object remains fully alive/functional for the rest of the current tick();
     * actual teardown happens once, in processPendingDestructions(), right after tick() returns.
     * Internal name deliberately NOT "destroyLayer" — ScriptEngine::destroyLayer(ScriptLayerHandle)
     * already exists as a wholly unrelated mechanism (CText's per-object text-script sandboxes).
     * @return true if `id` resolved to a live object (and was marked); false if not found.
     */
    bool destroyObject (int id);

    /**
     * Registers a DynamicValue that carries an AnimationMode::Single Timeline Animation
     * (WE_DOCS_REFERENCE.md section 13) for per-frame ticking. Mirrors ScriptEngine::queueScript's
     * registration shape (object identity kept alongside the value so a destroyed object's
     * entries can be cleaned up the same way ScriptEngine::forgetObject already does for
     * property-scripts) but needs no JS involvement -- ticking is pure elapsed-time math.
     * No-op if `value` has no animation or its mode isn't Single (Loop/Mirror are parsed but
     * intentionally left untouched for now, see AnimationTimeline.h).
     */
    void queueAnimation (Data::Model::DynamicValue& value, CObject& object);

protected:
    void renderFrame (const glm::ivec4& viewport) override;
    void updateMouse (const glm::ivec4& viewport);

    friend class CWallpaper;

private:
    Render::CObject* createObject (const Object& object);
    Render::CObject* dispatchObjectType (const Object& object);
    void addObjectToRenderOrder (const Object& object);

    /** Drains m_pendingDestruction, actually tearing down each marked object. Called once per
     * frame from renderFrame(), immediately after tick() returns. */
    void processPendingDestructions ();

    /** Advances every registered single-mode Timeline Animation by this frame's delta time and
     * pushes the interpolated value via DynamicValue::update(..., UpdateSource::Animation).
     * Called once per frame from renderFrame(), alongside the existing scriptEngine tick(). */
    void tickAnimations ();

    /** Removes every animation entry belonging to the given object -- tickAnimations() derefs
     * every entry's object pointer unconditionally, every frame, so this must happen before the
     * object is deleted (same reasoning as ScriptEngine::forgetObject, called from the same
     * processPendingDestructions() cleanup step). */
    void forgetObjectAnimations (const CObject& object);

    struct AnimatedPropertyEntry {
        Data::Model::DynamicValue* value = nullptr;
        CObject* object = nullptr;
        double elapsedTime = 0.0;
        bool completed = false;
    };

    std::unique_ptr<Scripting::ScriptEngine> m_scriptEngine;
    std::unique_ptr<Camera> m_camera;
    ObjectUniquePtr m_bloomObjectData;
    CObject* m_bloomObject = nullptr;
    std::vector<ObjectUniquePtr> m_dynamicObjectsData = {};
    int m_nextDynamicObjectId = -1000000;
    std::map<int, CObject*> m_objects = {};
    /** Object ids currently partway through createObject()'s dependency/parent walk -- lets a
     * scene-graph cycle (which closes before any of the recursive calls involved has returned,
     * so isn't yet in m_objects) be detected and broken instead of recursing forever. */
    std::unordered_set<int> m_objectsBeingResolved = {};
    std::vector<CObject*> m_objectsByRenderOrder = {};
    std::unordered_set<int> m_pendingDestruction = {};
    std::vector<DynamicValue*> m_scriptedValues = {};
    std::vector<AnimatedPropertyEntry> m_animatedProperties = {};
    glm::vec2 m_mousePosition = {};
    glm::vec2 m_mousePositionLast = {};
    glm::vec2 m_mousePositionNormalized = {};
    glm::vec2 m_parallaxDisplacement = {};
    std::shared_ptr<const CFBO> _rt_4FrameBuffer = nullptr;
    std::shared_ptr<const CFBO> _rt_8FrameBuffer = nullptr;
    std::shared_ptr<const CFBO> _rt_Bloom = nullptr;
    std::shared_ptr<const CFBO> _rt_shadowAtlas = nullptr;
};
} // namespace WallpaperEngine::Render::Wallpaper
