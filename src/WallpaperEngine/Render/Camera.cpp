#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Camera.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;

Camera::Camera (Wallpapers::CScene& scene, const SceneData::Camera& camera) :
    m_width (0), m_height (0), m_camera (camera), m_scene (scene) {
    // get the lookat position
    // TODO: ENSURE THIS IS ONLY USED WHEN NOT DOING AN ORTOGRAPHIC CAMERA AS IT THROWS OFF POINTS
    this->m_lookat = glm::lookAt (this->getEye (), this->getCenter (), this->getUp ());
}

Camera::~Camera () = default;

const glm::vec3& Camera::getCenter () const { return this->m_camera.configuration.center; }

const glm::vec3& Camera::getEye () const { return this->m_camera.configuration.eye; }

const glm::vec3& Camera::getUp () const { return this->m_camera.configuration.up; }

const glm::mat4& Camera::getProjection () const { return this->m_projection; }

const glm::mat4& Camera::getLookAt () const { return this->m_lookat; }

bool Camera::isOrthogonal () const { return this->m_isOrthogonal; }

Wallpapers::CScene& Camera::getScene () const { return this->m_scene; }

float Camera::getWidth () const { return this->m_width; }

float Camera::getHeight () const { return this->m_height; }

float Camera::getFov () const { return this->m_camera.projection.fov->value->getFloat (); }

float Camera::getNearZ () const { return this->m_camera.projection.nearz->value->getFloat (); }

float Camera::getFarZ () const { return this->m_camera.projection.farz->value->getFloat (); }

void Camera::setOrthogonalProjection (const float width, const float height) {
    this->m_width = width;
    this->m_height = height;

    float farz = this->m_camera.projection.farz->value->getFloat ();

    this->m_projection = glm::ortho<float> (-width / 2.0, width / 2.0, -height / 2.0, height / 2.0, 0.0f, farz);
    this->m_projection = glm::translate (this->m_projection, this->getEye ());
    this->m_isOrthogonal = true;
}

void Camera::setPerspectiveProjection (const float width, const float height, const float fov, const float near, const float far) {
    this->m_width = width;
    this->m_height = height;
    const float aspect = width / height;
    this->m_projection = glm::perspective (glm::radians (fov), aspect, near, far);
    this->m_isOrthogonal = false;
}

void Camera::applyObjectCamera (const glm::vec3& eye, float zoom) {
    // "camera" scene objects' zoom field is documented (WE's own ICamera scripting
    // interface) as 2D-scene-only. Never rebuild an orthographic projection over a
    // scene #55 already determined is perspective -- that silently clobbers it (#56).
    if (!this->m_isOrthogonal) {
	return;
    }

    if (zoom <= 0.0f)
	zoom = 1.0f;

    float w = this->m_width / zoom;
    float h = this->m_height / zoom;

    // Use nearz=0 to match setOrthogonalProjection: scene nearz (typically 0.01)
    // clips 2D geometry at z=0 because the projection and lookAt z-translations cancel,
    // leaving effective z=0 which falls just below nearz and gets depth-clipped.
    const float nearz = 0.0f;
    float farz = this->m_camera.projection.farz->value->getFloat ();

    // eye arrives in WE-space (top-left = 0,0, Y-down). Convert to GL-centered
    // (center = 0,0, Y-up) before building the projection translate offset.
    float glX = eye.x - this->m_width / 2.0f;
    float glY = -(eye.y - this->m_height / 2.0f);
    const glm::vec3 sceneEye = this->getEye ();
    // Match setOrthogonalProjection's approach: use scene camera eye to cancel the
    // lookAt translation, then subtract the camera-object's GL-space pan.
    const glm::vec3 offset (sceneEye.x - glX, sceneEye.y - glY, sceneEye.z);

    this->m_projection = glm::ortho<float> (-w / 2.0f, w / 2.0f, -h / 2.0f, h / 2.0f, nearz, farz);
    this->m_projection = glm::translate (this->m_projection, offset);
}