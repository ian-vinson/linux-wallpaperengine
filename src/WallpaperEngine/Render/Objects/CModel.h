#pragma once

#include "CRenderable.h"
#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Render/Objects/Effects/CPass.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"
#include "WallpaperEngine/Scripting/ScriptableObject.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <vector>

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Data::Model;

namespace WallpaperEngine::Render::Objects {

// Renders a static (non-skeletal) Wallpaper Engine "model"-keyed 3D scene object -- real mesh
// geometry with a full 3D world-space transform, viewed through the scene's real camera
// projection. Distinct from CImage: geometry is an arbitrary mesh (not a flat quad), the
// transform is a genuine 3D TRS (not a 2D billboard with Z-only rotation), and each sub-mesh
// gets its own CPass/material/texture rather than CImage's single-material multi-effect-pass
// pipeline. Rigged/animated models never reach this class -- see ObjectParser::parseModel3D().
class CModel final : public CRenderable, public Scripting::ScriptableObject {
    friend CObject;

public:
    CModel (Wallpapers::CScene& scene, const Model3D& model);
    ~CModel () override;

    void setup () override;
    void render () override;

    [[nodiscard]] const Model3D& getModel () const;

    [[nodiscard]] const float& getBrightness () const override;
    [[nodiscard]] const float& getUserAlpha () const override;
    [[nodiscard]] const float& getAlpha () const override;
    [[nodiscard]] const glm::vec3& getColor () const override;
    [[nodiscard]] glm::vec4 getColor4 () const override;
    [[nodiscard]] const glm::vec3& getCompositeColor () const override;

private:
    struct SubMesh {
	GLuint vao = 0;
	GLuint vboPositions = 0;
	GLuint vboNormals = 0;
	GLuint vboUVs = 0;
	GLuint ebo = 0;
	GLsizei indexCount = 0;
	Effects::CPass* pass = nullptr;
	std::shared_ptr<FBOProvider> fboProvider;
	GLint prevVAO = 0;
    };

    void updateModelMatrix ();

    const Model3D& m_model;
    std::vector<SubMesh> m_subMeshes;

    glm::mat4 m_modelMatrix { 1.0f };
    glm::mat4 m_viewProjectionMatrix { 1.0f };
    glm::mat3 m_normalModelMatrix { 1.0f };

    float m_brightness = 1.0f;
    float m_alpha = 1.0f;
    glm::vec3 m_color { 1.0f, 1.0f, 1.0f };
};
}
