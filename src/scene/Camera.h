#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    void setPosition(const glm::vec3& pos) { position = pos; }
    void setTarget(const glm::vec3& t) { target = t; }
    void setUp(const glm::vec3& u) { up = u; }
    void setFOV(float degrees) { fov = degrees; }
    void setClipPlanes(float near, float far) { nearPlane = near; farPlane = far; }

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, target, up);
    }

    glm::mat4 getProjectionMatrix(float aspectRatio) const {
        auto proj = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
        proj[1][1] *= -1; // Vulkan Y-flip (GLM uses OpenGL convention)
        return proj;
    }

    const glm::vec3& getPosition() const { return position; }
    const glm::vec3& getTarget() const { return target; }
    float getFOV() const { return fov; }

private:
    glm::vec3 position{10.0f, 15.0f, -10.0f};
    glm::vec3 target{-50.0f, 0.0f, 50.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float fov = 88.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
};
