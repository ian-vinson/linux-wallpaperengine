#include "AnimationTimeline.h"

#include <cmath>

using namespace WallpaperEngine::Data::Model;

namespace {
/**
 * Evaluates a single channel's keyframes at currentFrame (in frame units, matching each
 * keyframe's own "frame" field). Clamps to the first/last keyframe's value outside the channel's
 * own range -- this is what gives AnimationMode::Single its documented "hold forever" behavior.
 *
 * Non-default tangent handles (roughly half of real corpus keyframes, per #54's scoping scan) get
 * a real cubic-Bezier-in-time evaluation: the handle (dFrame, dValue) is a tangent DIRECTION
 * vector, converted to a Bezier control point via the standard 1/3-tangent rule, then the curve's
 * time axis is inverted (solve bezierX(u) == t for u, then evaluate bezierY(u)) the same way
 * CSS's cubic-bezier() timing function is evaluated. The exact scale WE's own editor uses for
 * these handles isn't reverse-engineered (no decompiled client available) -- this is a reasonable,
 * standard interpretation verified against real corpus examples for correct endpoints and smooth,
 * monotonic-where-expected easing shapes, not confirmed byte-for-byte against the real client.
 */
float evaluateChannel (const std::vector<AnimationKeyframe>& keyframes, float currentFrame) {
    if (keyframes.empty ()) {
        return 0.0f;
    }

    if (keyframes.size () == 1 || currentFrame <= keyframes.front ().frame) {
        return keyframes.front ().value;
    }

    if (currentFrame >= keyframes.back ().frame) {
        return keyframes.back ().value;
    }

    size_t i = 0;
    while (i + 1 < keyframes.size () && keyframes[i + 1].frame <= currentFrame) {
        ++i;
    }

    const AnimationKeyframe& a = keyframes[i];
    const AnimationKeyframe& b = keyframes[i + 1];
    const float span = b.frame - a.frame;

    if (span <= 0.0f) {
        return b.value;
    }

    const float t = (currentFrame - a.frame) / span;

    // A handle counts as a real easing curve only if it's enabled AND actually deflects the
    // curve (non-zero value component) -- WE's default/unauthored handle is effectively linear
    // (a straight direction with y == 0), so treat that the same as a disabled handle.
    const bool frontIsLinear = !a.front.enabled || a.front.y == 0.0f;
    const bool backIsLinear = !b.back.enabled || b.back.y == 0.0f;

    if (frontIsLinear && backIsLinear) {
        return a.value + (b.value - a.value) * t;
    }

    constexpr float kTangentScale = 1.0f / 3.0f;
    const glm::vec2 p0 (0.0f, a.value);
    const glm::vec2 p3 (1.0f, b.value);
    glm::vec2 p1 = p0;
    if (a.front.enabled) {
        p1 = p0 + glm::vec2 (a.front.x, a.front.y) * kTangentScale;
    }
    glm::vec2 p2 = p3;
    if (b.back.enabled) {
        p2 = p3 + glm::vec2 (b.back.x, b.back.y) * kTangentScale;
    }

    const auto bezier = [] (float v0, float v1, float v2, float v3, float u) {
        const float mu = 1.0f - u;
        return mu * mu * mu * v0 + 3.0f * mu * mu * u * v1 + 3.0f * mu * u * u * v2 + u * u * u * v3;
    };
    const auto bezierDerivative = [] (float v0, float v1, float v2, float v3, float u) {
        const float mu = 1.0f - u;
        return 3.0f * mu * mu * (v1 - v0) + 6.0f * mu * u * (v2 - v1) + 3.0f * u * u * (v3 - v2);
    };

    // Newton-Raphson to solve bezierX(u) == t; falls back to the linear guess (t itself) if it
    // diverges outside [0,1] -- the observed corpus handle magnitudes (roughly -1.2..1.2) keep
    // the x(u) curve well-behaved/monotonic in practice, so this converges in a handful of steps.
    float u = t;
    for (int iter = 0; iter < 8; ++iter) {
        const float x = bezier (p0.x, p1.x, p2.x, p3.x, u) - t;
        const float dx = bezierDerivative (p0.x, p1.x, p2.x, p3.x, u);

        if (std::abs (dx) < 1e-6f) {
            break;
        }

        const float nextU = u - x / dx;

        if (nextU < 0.0f || nextU > 1.0f) {
            break;
        }

        u = nextU;

        if (std::abs (x) < 1e-5f) {
            break;
        }
    }

    u = glm::clamp (u, 0.0f, 1.0f);

    return bezier (p0.y, p1.y, p2.y, p3.y, u);
}
} // namespace

glm::vec4 AnimationTimeline::evaluate (double elapsedSeconds) const {
    const float currentFrame = static_cast<float> (elapsedSeconds) * this->fps;
    glm::vec4 result (0.0f);

    for (size_t i = 0; i < this->channels.size () && i < 4; ++i) {
        result[static_cast<glm::vec4::length_type> (i)] = evaluateChannel (this->channels[i], currentFrame);
    }

    if (this->relative) {
        result += this->baseValue;
    }

    return result;
}
