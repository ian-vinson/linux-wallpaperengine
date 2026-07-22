#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace WallpaperEngine::Data::Model {

/**
 * One end of a keyframe's Bezier tangent handle, in (frame, value) space. Matches WE's own
 * per-keyframe "back"/"front" handle JSON shape -- "enabled" false means the curve editor treats
 * this side as a flat/linear handle rather than a custom-authored tangent.
 */
struct AnimationTangent {
    bool enabled = false;
    float x = 0.0f;
    float y = 0.0f;
};

struct AnimationKeyframe {
    float frame = 0.0f;
    float value = 0.0f;
    AnimationTangent back;
    AnimationTangent front;
};

enum class AnimationMode {
    Unknown,
    Single,
    Loop,
    Mirror,
};

/**
 * A parsed Wallpaper Engine "Timeline Animation" (editor: property cogwheel -> "Bind Timeline
 * Animation", WE_DOCS_REFERENCE.md section 13). One channel per vector component (c0/c1/c2/...);
 * scalar properties have exactly one channel.
 *
 * Only AnimationMode::Single is currently ticked (see CScene::tickAnimations) -- Loop/Mirror are
 * parsed and stored here (so the data is available once that's implemented) but intentionally
 * never evaluated in this pass, matching this engine's pre-existing behavior for them (previously
 * silently discarded entirely) so this stays a purely additive change with no risk to existing
 * loop/mirror wallpapers.
 */
struct AnimationTimeline {
    std::vector<std::vector<AnimationKeyframe>> channels;
    AnimationMode mode = AnimationMode::Unknown;
    float fps = 30.0f;
    float length = 0.0f;
    bool relative = false;
    /** The property's static "value" field, snapshotted at parse time -- added back in when
     * relative == true, so the animation composes onto the base value rather than replacing it. */
    glm::vec4 baseValue = {};

    /**
     * Evaluates every channel at the given elapsed time (in seconds) and combines them into a
     * vec4 (unused channels left at 0, or added to baseValue's corresponding component when
     * relative). Each channel independently clamps to its own first/last keyframe outside its
     * own range, so for AnimationMode::Single this naturally implements WE's documented
     * "hold final state forever" once elapsedSeconds exceeds the timeline's duration.
     */
    [[nodiscard]] glm::vec4 evaluate (double elapsedSeconds) const;

    /** Total timeline duration in seconds (length is in frames). */
    [[nodiscard]] double durationSeconds () const {
        return this->fps > 0.0f ? static_cast<double> (this->length) / static_cast<double> (this->fps) : 0.0;
    }
};

} // namespace WallpaperEngine::Data::Model
