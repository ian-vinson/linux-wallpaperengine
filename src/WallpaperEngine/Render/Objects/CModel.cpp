#include "CModel.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "WallpaperEngine/Logging/Log.h"

using namespace WallpaperEngine::Render::Objects;
using namespace WallpaperEngine::Render::Objects::Effects;
using namespace WallpaperEngine::Scripting;

CModel::CModel (Wallpapers::CScene& scene, const Model3D& model) :
    CObject (scene, model), CRenderable (scene, model, *model.material), ScriptableObject (scene, model),
    m_model (model) {
    // register any scriptable properties in use on this object -- own copies mirror CImage's
    // precedent (registerProperty() is last-wins as of #46, so this object's own scale/angles/
    // visible correctly take precedence over the base ScriptableObject ctor's generic ones)
    this->registerProperty ("origin", *model.origin->value);
    this->registerProperty ("scale", *model.scale->value);
    this->registerProperty ("angles", *model.angles->value);
    this->registerProperty ("visible", *model.visible->value);

    this->updateModelMatrix ();
}

CModel::~CModel () {
    for (auto& sub : this->m_subMeshes) {
	delete sub.pass;
	if (sub.vao != 0) {
	    glDeleteVertexArrays (1, &sub.vao);
	}
	if (sub.vboPositions != 0) {
	    glDeleteBuffers (1, &sub.vboPositions);
	}
	if (sub.vboNormals != 0) {
	    glDeleteBuffers (1, &sub.vboNormals);
	}
	if (sub.vboUVs != 0) {
	    glDeleteBuffers (1, &sub.vboUVs);
	}
	if (sub.ebo != 0) {
	    glDeleteBuffers (1, &sub.ebo);
	}
    }
}

const Model3D& CModel::getModel () const { return this->m_model; }

const float& CModel::getBrightness () const { return this->m_brightness; }
const float& CModel::getUserAlpha () const { return this->m_alpha; }
const float& CModel::getAlpha () const { return this->m_alpha; }
const glm::vec3& CModel::getColor () const { return this->m_color; }
glm::vec4 CModel::getColor4 () const { return { this->m_color, this->m_alpha }; }
const glm::vec3& CModel::getCompositeColor () const { return this->m_color; }

void CModel::updateModelMatrix () {
    const glm::vec3 origin = this->getModel ().origin->value->getVec3 ();
    const glm::vec3 scale = this->getModel ().scale->value->getVec3 ();
    const glm::vec3 angles = this->getModel ().angles->value->getVec3 ();

    glm::mat4 model (1.0f);
    model = glm::translate (model, origin);
    // Same rotation order/axis-negation convention as CParticle::updateMatrices() -- accounts for
    // this engine's Y-flipped coordinate system.
    model = glm::rotate (model, -angles.z, glm::vec3 (0.0f, 0.0f, 1.0f));
    model = glm::rotate (model, angles.y, glm::vec3 (0.0f, 1.0f, 0.0f));
    model = glm::rotate (model, -angles.x, glm::vec3 (1.0f, 0.0f, 0.0f));
    model = glm::scale (model, scale);

    this->m_modelMatrix = model;
    this->m_normalModelMatrix = glm::inverseTranspose (glm::mat3 (model));
    this->m_viewProjectionMatrix = this->getScene ().getCamera ().getProjection () * this->getScene ().getCamera ().getLookAt ();
}

void CModel::setup () {
    const auto fboProvider = std::make_shared<FBOProvider> (this);

    // Sized once upfront so every SubMesh has a stable address for the rest of this object's
    // lifetime -- the geometry callbacks below capture a reference into this vector, which would
    // dangle if a later push_back triggered a reallocation.
    this->m_subMeshes.resize (this->m_model.subMeshes.size ());

    for (size_t index = 0; index < this->m_model.subMeshes.size (); index++) {
	const auto& subMeshData = this->m_model.subMeshes[index];
	const auto& materialPass = *this->m_model.material->passes[index];
	SubMesh& sub = this->m_subMeshes[index];

	sub.fboProvider = fboProvider;
	sub.indexCount = static_cast<GLsizei> (subMeshData.indices.size ());

	sub.pass = new CPass (*this, fboProvider, materialPass, std::nullopt, std::nullopt, std::nullopt);
	sub.pass->setDestination (this->getScene ().getFBO ());
	sub.pass->setModelMatrix (&this->m_modelMatrix);
	sub.pass->setViewProjectionMatrix (&this->m_viewProjectionMatrix);
	// g_NormalModelMatrix (mat3) has no public CPass::addUniform() overload, and with
	// LIGHTING/REFLECTION forced off above, the shader's only consumer of the normal it
	// produces is dead code under those combos -- deliberately left unset rather than
	// reaching into CPass internals for a value that provably never affects output here.
	sub.pass->addUniform ("g_EyePosition", &this->getScene ().getCamera ().getEye ());

	const GLuint program = sub.pass->getProgramID ();

	GLint prevVAO = 0;
	glGetIntegerv (GL_VERTEX_ARRAY_BINDING, &prevVAO);

	glGenVertexArrays (1, &sub.vao);
	glGenBuffers (1, &sub.vboPositions);
	glGenBuffers (1, &sub.vboNormals);
	glGenBuffers (1, &sub.vboUVs);
	glGenBuffers (1, &sub.ebo);

	glBindVertexArray (sub.vao);

	glBindBuffer (GL_ARRAY_BUFFER, sub.vboPositions);
	glBufferData (
	    GL_ARRAY_BUFFER, static_cast<GLsizeiptr> (subMeshData.positions.size () * sizeof (float)),
	    subMeshData.positions.data (), GL_STATIC_DRAW
	);
	const GLint positionLoc = glGetAttribLocation (program, "a_Position");
	if (positionLoc >= 0) {
	    glEnableVertexAttribArray (positionLoc);
	    glVertexAttribPointer (positionLoc, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	}

	glBindBuffer (GL_ARRAY_BUFFER, sub.vboNormals);
	glBufferData (
	    GL_ARRAY_BUFFER, static_cast<GLsizeiptr> (subMeshData.normals.size () * sizeof (float)),
	    subMeshData.normals.data (), GL_STATIC_DRAW
	);
	const GLint normalLoc = glGetAttribLocation (program, "a_Normal");
	if (normalLoc >= 0) {
	    glEnableVertexAttribArray (normalLoc);
	    glVertexAttribPointer (normalLoc, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	}

	glBindBuffer (GL_ARRAY_BUFFER, sub.vboUVs);
	glBufferData (
	    GL_ARRAY_BUFFER, static_cast<GLsizeiptr> (subMeshData.uvs.size () * sizeof (float)), subMeshData.uvs.data (),
	    GL_STATIC_DRAW
	);
	const GLint texCoordLoc = glGetAttribLocation (program, "a_TexCoord");
	if (texCoordLoc >= 0) {
	    glEnableVertexAttribArray (texCoordLoc);
	    glVertexAttribPointer (texCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	}

	glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, sub.ebo);
	glBufferData (
	    GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr> (subMeshData.indices.size () * sizeof (uint16_t)),
	    subMeshData.indices.data (), GL_STATIC_DRAW
	);

	glBindVertexArray (static_cast<GLuint> (prevVAO));

	sub.pass->setGeometryCallback (
	    [&sub] () {
		glGetIntegerv (GL_VERTEX_ARRAY_BINDING, &sub.prevVAO);
		glBindVertexArray (sub.vao);
	    },
	    [&sub] () { glDrawElements (GL_TRIANGLES, sub.indexCount, GL_UNSIGNED_SHORT, nullptr); },
	    [&sub] () { glBindVertexArray (static_cast<GLuint> (sub.prevVAO)); }
	);
    }

    sLog.out (
	"Loaded static model '", this->m_model.modelPath, "': ", this->m_subMeshes.size (), " sub-mesh(es)"
    );
}

void CModel::render () {
    this->advanceAnimationTime ();

    if (!this->getModel ().visible->value->getBool ()) {
	return;
    }

    this->updateModelMatrix ();

    for (const auto& sub : this->m_subMeshes) {
	sub.pass->render ();
    }
}
