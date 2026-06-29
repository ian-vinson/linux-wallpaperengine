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

    float nearz = this->m_camera.projection.nearz->value->getFloat ();
    float farz = this->m_camera.projection.farz->value->getFloat ();

    this->m_projection = glm::ortho<float> (-width / 2.0, width / 2.0, -height / 2.0, height / 2.0, nearz, farz);
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
    if (zoom <= 0.0f)
	zoom = 1.0f;

    float w = this->m_width / zoom;
    float h = this->m_height / zoom;

    float nearz = this->m_camera.projection.nearz->value->getFloat ();
    float farz = this->m_camera.projection.farz->value->getFloat ();

    // glm::translate(ortho, v) centers the view at world position -v.
    // eye is the camera object's world-space look-at position, so we negate
    // XY to convert "look-at point" to the translate offset the projection expects.
    // Z is kept positive so scene objects at z=0 map to z_eff = eye.z which
    // stays within [nearz, farz] rather than being depth-clipped.
    const glm::vec3 offset (-eye.x, -eye.y, eye.z);

    this->m_projection = glm::ortho<float> (-w / 2.0f, w / 2.0f, -h / 2.0f, h / 2.0f, nearz, farz);
    this->m_projection = glm::translate (this->m_projection, offset);
}