#include "CRenderable.h"

#include "WallpaperEngine/Data/Model/Material.h"
#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Data/Parsers/MaterialParser.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render::Objects;
using namespace WallpaperEngine::Render::Objects::Effects;
using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Builders;

extern float g_Time;
extern float g_TimeLast;

CRenderable::CRenderable (Wallpapers::CScene& scene, const Object& object, const Material& material) :
    CObject (scene, object), Render::FBOProvider (&scene), m_material (material) { }

void CRenderable::detectTexture () {
    if (TextureMap* textures = &(*this->m_material.passes.begin ())->textures; !textures->empty ()) {
	std::string textureName = textures->begin ()->second;

	if (textureName.find ("_rt_") == 0 || textureName.find ("_alias_") == 0) {
	    this->m_texture = this->getScene ().findFBO (textureName);
	} else {
	    this->m_texture = this->getContext ().resolveTexture (textureName);
	}
    }
}

void CRenderable::setup () {
    CObject::setup ();

    // calculate full animation time (if any)
    this->m_animationTime = 0.0f;

    for (const auto& cur : this->getTexture ()->getFrames ()) {
	this->m_animationTime += cur->frametime;
    }

    // Seed with the current global render clock rather than 0.0: since every frame from here on
    // advances m_animationElapsedTime by the exact same delta g_Time itself advances by
    // (advanceAnimationTime(), rate=1.0, never paused), this keeps elapsed == g_Time at all
    // times by construction — required so default (unscripted) playback stays pixel-identical
    // to the old global-clock-only behavior, for objects created at scene load (g_Time == 0 at
    // this point, same as the old implicit default) and for ones created later via
    // thisScene.createLayer() (g_Time already nonzero) alike.
    this->m_animationElapsedTime = g_Time;
}

void CRenderable::advanceAnimationTime () {
    if (this->m_animationPaused) {
	return;
    }

    this->m_animationElapsedTime += static_cast<double> (g_Time - g_TimeLast) * this->m_animationRate;
}

std::shared_ptr<const TextureProvider> CRenderable::getTexture () const { return this->m_texture; }

double CRenderable::getAnimationTime () const { return this->m_animationTime; }

double CRenderable::getAnimationElapsedTime () const { return this->m_animationElapsedTime; }

float CRenderable::getAnimationRate () const { return this->m_animationRate; }

void CRenderable::setAnimationRate (const float rate) { this->m_animationRate = rate; }

bool CRenderable::isAnimationPaused () const { return this->m_animationPaused; }

void CRenderable::setAnimationPaused (const bool paused) { this->m_animationPaused = paused; }

void CRenderable::stopAnimation () {
    this->m_animationElapsedTime = 0.0;
    this->m_animationPaused = true;
}

void CRenderable::setAnimationFrame (const uint32_t frameNumber) {
    // frameNumber here is a playback-sequence position (the same sense CPass's frame-selection
    // loop iterates texture->getFrames() in), not Frame::frameNumber — that field is documented
    // as "the image index of this frame" (which atlas image to show), not a sequential playback
    // index, so the two can diverge for animations that reuse or reorder atlas images.
    double cumulative = 0.0;
    uint32_t index = 0;

    for (const auto& frame : this->getTexture ()->getFrames ()) {
	if (index >= frameNumber) {
	    break;
	}

	cumulative += frame->frametime;
	index++;
    }

    this->m_animationElapsedTime = cumulative;
}