#pragma once

#include "ObjectAdapter.h"

#include <cstdint>
#include <glm/glm.hpp>
#include <string>

extern "C" {
#include "quickjs.h"
}

namespace WallpaperEngine::Scripting::Adapters {
/**
 * Exposes glm::mat3 to SceneScript as the `Mat3` class (see lib.sceneScript.d.ts) — a 2D affine
 * transform in homogeneous form (translation lives in the third column), following the same
 * plain-value opaque-struct pattern as Mat4Adapter (see its header for the rationale versus
 * VectorAdapter's DynamicValue-backed, exotic-property design).
 *
 * Depends on Mat4Adapter for fromMat4(), which is why Mat4 is constructed first in ScriptEngine.
 */
class Mat3Adapter : public ObjectAdapter {
  public:
    explicit Mat3Adapter (ScriptEngine& engine);
    ~Mat3Adapter () override;

    /** Builds a fresh Mat3 JS instance wrapping the given value. */
    JSValue wrap (const glm::mat3& value);

    /** Extracts the glm::mat3 backing a genuine Mat3 instance created by this adapter. */
    bool extract (JSValueConst value, glm::mat3& out) const;

    void releaseBeforeRuntimeShutdown ();

  private:
    JSValue m_prototype;
    std::string m_name;
    uint32_t m_instanceId;
};
} // namespace WallpaperEngine::Scripting::Adapters
