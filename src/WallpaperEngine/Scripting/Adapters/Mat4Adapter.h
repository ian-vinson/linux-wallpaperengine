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
 * Exposes glm::mat4 to SceneScript as the `Mat4` class (see lib.sceneScript.d.ts).
 *
 * Unlike VectorAdapter's Vec2/3/4 (which are DynamicValue-backed so they can serve as
 * scriptable scene-property values), Mat4 has no scene.json property use case — the opaque
 * struct backing each instance embeds the glm::mat4 directly by value, with no adapter-owned
 * lifecycle tracking (VectorAdapter's m_values/free(id) machinery has no equivalent here).
 *
 * Also unlike Vec2/3/4, Mat4 has only one non-method property (`m`), so it uses an ordinary
 * getter/setter pair on the prototype instead of class-level exotic get_property/set_property —
 * exotic methods exist in VectorAdapter specifically to arbitrate many colliding names (x/y/z/w
 * vs. prototype methods), which doesn't apply here.
 */
class Mat4Adapter : public ObjectAdapter {
  public:
    explicit Mat4Adapter (ScriptEngine& engine);
    ~Mat4Adapter () override;

    /** Builds a fresh Mat4 JS instance wrapping the given value. */
    JSValue wrap (const glm::mat4& value);

    /** Extracts the glm::mat4 backing a genuine Mat4 instance created by this adapter. */
    bool extract (JSValueConst value, glm::mat4& out) const;

    void releaseBeforeRuntimeShutdown ();

  private:
    JSValue m_prototype;
    std::string m_name;
    uint32_t m_instanceId;
};
} // namespace WallpaperEngine::Scripting::Adapters
