#pragma once

#include <string>

#include <glm/vec3.hpp>

#include "WallpaperEngine/Render/Helpers/ContextAware.h"

#include "WallpaperEngine/Render/Wallpapers/CScene.h"

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Render {
class CObject : public Helpers::ContextAware, public TypeCaster {
public:
    CObject (Wallpapers::CScene& scene, const Object& object);
    virtual ~CObject () override = default;

    virtual void setup ();
    virtual void render ();

    [[nodiscard]] Wallpapers::CScene& getScene () const;
    [[nodiscard]] const AssetLocator& getAssetLocator () const;
    [[nodiscard]] int getId () const;
    [[nodiscard]] const Object& getObject () const;

protected:
    struct ResolvedTransform {
	glm::vec3 origin;
	glm::vec3 scale;
	float angle;
    };

    /**
     * Walks the object's parent chain (if any) and folds each ancestor's local transform onto
     * its parent's, so a child's own origin/scale/angle compose relative to a
     * scaled/rotated/repositioned parent instead of being read as absolute scene coordinates.
     * Shared by every CObject-derived render type (CImage originally, now also CText) so parent
     * nesting behaves the same way regardless of which object type is involved.
     */
    [[nodiscard]] ResolvedTransform resolveTransform (const Object& object) const;

    /**
     * Computes the object's own transform (origin/scale/angle) without walking the
     * parent chain. Used as the per-node step of resolveTransform.
     */
    [[nodiscard]] static ResolvedTransform localTransform (const Object& object);

private:
    Wallpapers::CScene& m_scene;
    const Object& m_object;
};
} // namespace WallpaperEngine::Render