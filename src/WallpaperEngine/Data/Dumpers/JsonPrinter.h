#pragma once

#include <nlohmann/json.hpp>

#include "WallpaperEngine/Data/Model/Types.h"

namespace WallpaperEngine::Data::Dumpers {
using namespace WallpaperEngine::Data::Model;

/**
 * Structured (JSON) equivalent of StringPrinter, covering the same data for
 * --dump-structure --format=json. See StringPrinter for the plain-text default.
 */
class JsonPrinter {
public:
    nlohmann::json printWallpaper (const Wallpaper& wallpaper);
    nlohmann::json printObject (const Object& object);
    nlohmann::json printImage (const Image& image);
    nlohmann::json printSound (const Sound& sound);
    nlohmann::json printText (const Text& text);
    nlohmann::json printParticle (const Particle& particle);
    nlohmann::json printCameraObject (const CameraObject& camera);
    nlohmann::json printModel (const ModelStruct& model);
    nlohmann::json printImageEffect (const ImageEffect& imageEffect);
    nlohmann::json printImageEffectPassOverride (const ImageEffectPassOverride& imageEffectPass);
    nlohmann::json printFBO (const FBO& fbo);
    nlohmann::json printMaterial (const Material& material);
    nlohmann::json printMaterialPass (const MaterialPass& materialPass);
    nlohmann::json printEffect (const Effect& effect);
    nlohmann::json printEffectPass (const EffectPass& effectPass);
};
} // namespace WallpaperEngine::Data::Dumpers
