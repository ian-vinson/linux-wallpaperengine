#pragma once

#include "WallpaperEngine/Data/JSON.h"
#include "WallpaperEngine/Data/Model/AnimationTimeline.h"
#include "WallpaperEngine/Data/Model/Types.h"

namespace WallpaperEngine::Data::Parsers {
using json = WallpaperEngine::Data::JSON::JSON;
using namespace WallpaperEngine::Data::Model;

class DynamicValueParser {
public:
    static Model::DynamicValueUniquePtr parse (const json& data, const Properties& properties, bool expectColor);

private:
    /** Parses a property's "animation" sub-object (WE's Timeline Animation feature) into an
     * AnimationTimeline. Parses every mode (single/loop/mirror) -- only mode == Single is
     * actually ticked today (see CScene::tickAnimations), but storing the rest now avoids a
     * second parsing pass when Loop/Mirror support is added later. */
    static Model::AnimationTimeline parseAnimation (const json& data, const glm::vec4& baseValue);
};
}