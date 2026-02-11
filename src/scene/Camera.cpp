#include "Camera.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cmath>

Camera::Camera() {
    updateCameraVectors();
}

void Camera::processInput(GLFWwindow* window, float deltaTime) {
    // Speed modifiers
    float speed = moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        speed *= speedBoost;
    }

    // Right mouse button + WASD = Fly camera
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        // WASD movement
        glm::vec3 forward = glm::normalize(target - position);
        glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
        glm::vec3 localUp = glm::normalize(glm::cross(right, forward));

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            position += forward * speed * deltaTime;
            target += forward * speed * deltaTime;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            position -= forward * speed * deltaTime;
            target -= forward * speed * deltaTime;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            position -= right * speed * deltaTime;
            target -= right * speed * deltaTime;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            position += right * speed * deltaTime;
            target += right * speed * deltaTime;
        }
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
            position += localUp * speed * deltaTime;
            target += localUp * speed * deltaTime;
        }
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
            position -= localUp * speed * deltaTime;
            target -= localUp * speed * deltaTime;
        }
    }
}

void Camera::processMouseMovement(float xoffset, float yoffset, CameraMode mode) {
    // Invert horizontal mouse movement for more intuitive control
    xoffset = -xoffset;

    if (mode == CameraMode::ORBIT) {
        // Alt + Left Mouse = Orbit around focus point
        orbitAroundTarget(xoffset, yoffset);
    }
    else if (mode == CameraMode::PAN) {
        // Middle Mouse = Pan (translate parallel to view)
        pan(xoffset, yoffset);
    }
    else if (mode == CameraMode::ZOOM_DOLLY) {
        // Alt + Right Mouse = Zoom/Dolly
        dolly(yoffset);
    }
    else if (mode == CameraMode::FLY) {
        // Right Mouse = Free look (rotate around position)
        freeLook(xoffset, yoffset);
    }
}

void Camera::processMouseScroll(float yoffset) {
    // Mouse wheel = Adjust camera movement speed (UE5 style)
    moveSpeed += yoffset * 5.0f;  // 5 units per scroll notch
    moveSpeed = glm::clamp(moveSpeed, 1.0f, 500.0f);  // Clamp between 1 and 500

    std::cout << "Camera speed: " << moveSpeed << " units/sec" << std::endl;
}

void Camera::setAspectRatio(float aspect) {
    aspectRatio = aspect;
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(position, target, up);
}

glm::mat4 Camera::getProjectionMatrix() const {
    auto proj = glm::perspectiveRH_ZO(glm::radians(fov), aspectRatio, nearPlane, farPlane);
    proj[1][1] *= -1; // Vulkan Y-flip
    return proj;
}

void Camera::saveState(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to save camera state to " << filepath << std::endl;
        return;
    }

    // Write camera state as simple text format
    file << "# Camera State\n";
    file << "position " << position.x << " " << position.y << " " << position.z << "\n";
    file << "target " << target.x << " " << target.y << " " << target.z << "\n";
    file << "up " << up.x << " " << up.y << " " << up.z << "\n";
    file << "fov " << fov << "\n";
    file << "nearPlane " << nearPlane << "\n";
    file << "farPlane " << farPlane << "\n";

    file.close();
    std::cout << "Camera state saved to " << filepath << std::endl;
}

void Camera::loadState(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cout << "No camera state file found (" << filepath << "), using defaults." << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string key;
        iss >> key;

        if (key == "position") {
            iss >> position.x >> position.y >> position.z;
        }
        else if (key == "target") {
            iss >> target.x >> target.y >> target.z;
        }
        else if (key == "up") {
            iss >> up.x >> up.y >> up.z;
        }
        else if (key == "fov") {
            iss >> fov;
        }
        else if (key == "nearPlane") {
            iss >> nearPlane;
        }
        else if (key == "farPlane") {
            iss >> farPlane;
        }
    }

    file.close();
    updateCameraVectors();
    std::cout << "Camera state loaded from " << filepath << std::endl;
}

void Camera::orbitAroundTarget(float xoffset, float yoffset) {
    // Calculate current distance and direction
    glm::vec3 direction = position - target;
    float distance = glm::length(direction);
    direction = glm::normalize(direction);

    // Convert to spherical coordinates
    float radius = distance;
    float theta = atan2(direction.x, direction.z); // Azimuth angle
    float phi = acos(direction.y / radius);         // Polar angle

    // Apply rotation deltas
    theta += xoffset * mouseSensitivity;
    phi -= yoffset * mouseSensitivity;

    // Clamp phi to avoid gimbal lock (keep away from poles)
    phi = glm::clamp(phi, 0.01f, glm::pi<float>() - 0.01f);

    // Convert back to Cartesian coordinates
    direction.x = radius * sin(phi) * sin(theta);
    direction.y = radius * cos(phi);
    direction.z = radius * sin(phi) * cos(theta);

    position = target + direction;
    updateCameraVectors();
}

void Camera::pan(float xoffset, float yoffset) {
    // Calculate right and up vectors
    glm::vec3 forward = glm::normalize(target - position);
    glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
    glm::vec3 localUp = glm::normalize(glm::cross(right, forward));

    // Pan speed based on distance to target
    float distance = glm::length(target - position);
    float panSpeed = distance * 0.001f;

    // Move both position and target
    glm::vec3 offset = -right * xoffset * panSpeed + localUp * yoffset * panSpeed;
    position += offset;
    target += offset;
}

void Camera::dolly(float yoffset) {
    // Move camera closer/farther from target
    glm::vec3 direction = glm::normalize(position - target);
    float distance = glm::length(position - target);

    distance += yoffset * scrollSpeed;
    distance = glm::clamp(distance, 1.0f, 500.0f);

    position = target + direction * distance;
}

void Camera::freeLook(float xoffset, float yoffset) {
    // Rotate around camera position (not target)
    glm::vec3 direction = glm::normalize(target - position);
    float distance = glm::length(target - position);

    // Convert to spherical coordinates relative to position
    float theta = atan2(direction.x, direction.z);
    float phi = acos(direction.y);

    // Apply rotation
    theta += xoffset * mouseSensitivity;
    phi -= yoffset * mouseSensitivity;

    // Clamp phi
    phi = glm::clamp(phi, 0.01f, glm::pi<float>() - 0.01f);

    // Convert back to Cartesian
    direction.x = sin(phi) * sin(theta);
    direction.y = cos(phi);
    direction.z = sin(phi) * cos(theta);

    target = position + glm::normalize(direction) * distance;
    updateCameraVectors();
}

void Camera::updateCameraVectors() {
    glm::vec3 forward = glm::normalize(target - position);
    glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
    up = glm::normalize(glm::cross(right, forward));
}
