#pragma once
#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Render/FBOProvider.h"
#include "WallpaperEngine/Render/Objects/Effects/CPass.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"

#include "WallpaperEngine/Render/Shaders/Shader.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;

namespace WallpaperEngine::Render::Objects {
class CRenderable : virtual public CObject, public FBOProvider {
    friend CObject;

public:
    CRenderable (Wallpapers::CScene& scene, const Object& object, const Material& material);

    [[nodiscard]] std::shared_ptr<const TextureProvider> getTexture () const;

    [[nodiscard]] double getAnimationTime () const;

    // ITextureAnimation scripting surface (thisLayer.getTextureAnimation(), see
    // ScriptableObjectAdapter.cpp). getAnimationElapsedTime() replaces the global render clock
    // as CPass::resolveTextureAnimationState()'s time source for frame selection.
    [[nodiscard]] double getAnimationElapsedTime () const;
    [[nodiscard]] float getAnimationRate () const;
    void setAnimationRate (float rate);
    [[nodiscard]] bool isAnimationPaused () const;
    void setAnimationPaused (bool paused);
    // Resets to frame 0 and pauses ("resets internal playback timer", matching ISoundLayer's
    // stop() language) — a subsequent play() resumes advancing from there.
    void stopAnimation ();
    // Jumps to the given frame's cumulative start time without touching the paused state, so
    // setFrame() while playing continues animating from the new position.
    void setAnimationFrame (uint32_t frameNumber);

    void setup () override;

    [[nodiscard]] virtual const float& getBrightness () const = 0;
    [[nodiscard]] virtual const float& getUserAlpha () const = 0;
    [[nodiscard]] virtual const float& getAlpha () const = 0;
    [[nodiscard]] virtual const glm::vec3& getColor () const = 0;
    [[nodiscard]] virtual glm::vec4 getColor4 () const = 0;
    [[nodiscard]] virtual const glm::vec3& getCompositeColor () const = 0;

protected:
    void detectTexture ();
    // Advances m_animationElapsedTime by this frame's real delta (scaled by m_animationRate)
    // unless paused. Must be called once per object per frame from each concrete subclass's
    // render() — there's no single shared render() override on CRenderable itself to hook.
    void advanceAnimationTime ();

    double m_animationTime = 0.0;
    double m_animationElapsedTime = 0.0;
    float m_animationRate = 1.0f;
    bool m_animationPaused = false;

    std::shared_ptr<const TextureProvider> m_texture = nullptr;
    const Material& m_material;
};
}
