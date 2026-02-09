#pragma once

#include <glm/glm.hpp>
#include <string>

struct GLFWwindow;

enum class CameraMode {
    NONE,
    FLY,        // Right mouse button (free look)
    ORBIT,      // Alt + Left mouse button
    PAN,        // Middle mouse button
    ZOOM_DOLLY  // Alt + Right mouse button
};

class Camera {
public:
    Camera();

    // Input processing (call every frame)
    void processInput(GLFWwindow* window, float deltaTime);
    void processMouseMovement(float xoffset, float yoffset, CameraMode mode);
    void processMouseScroll(float yoffset);

    // Setters
    void setPosition(const glm::vec3& pos) { position = pos; }
    void setTarget(const glm::vec3& t) { target = t; }
    void setUp(const glm::vec3& u) { up = u; }
    void setFOV(float degrees) { fov = degrees; }
    void setClipPlanes(float near, float far) { nearPlane = near; farPlane = far; }
    void setAspectRatio(float aspect);

    // Getters
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    const glm::vec3& getPosition() const { return position; }
    const glm::vec3& getTarget() const { return target; }
    float getFOV() const { return fov; }

    // State persistence
    void saveState(const std::string& filepath) const;
    void loadState(const std::string& filepath);

private:
    void orbitAroundTarget(float xoffset, float yoffset);
    void pan(float xoffset, float yoffset);
    void dolly(float yoffset);
    void freeLook(float xoffset, float yoffset);
    void updateCameraVectors();

    // Camera attributes
    glm::vec3 position{10.0f, 15.0f, -10.0f};
    glm::vec3 target{-50.0f, 0.0f, 50.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::vec3 worldUp{0.0f, 1.0f, 0.0f};

    float fov = 88.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    float aspectRatio = 16.0f / 9.0f;

    // Camera settings
    float moveSpeed = 50.0f;          // Units per second
    float mouseSensitivity = 0.005f;  // Radians per pixel
    float scrollSpeed = 5.0f;         // Distance per scroll notch
    float speedBoost = 3.0f;          // Shift multiplier
};
