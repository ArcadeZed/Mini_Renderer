#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Transform {
public:
    glm::mat4 getModelMatrix() const {
        // If custom matrix is set (e.g., from glTF node), use it directly
        if (useCustomMatrix) {
            return customMatrix;
        }

        // Otherwise compose from TRS components
        glm::mat4 model(1.0f);
        model = glm::translate(model, position);
        model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0, 0, 1));
        model = glm::scale(model, scale);
        return model;
    }

    // Set a custom 4x4 matrix (e.g., from glTF node hierarchy)
    void setCustomMatrix(const glm::mat4& matrix) {
        customMatrix = matrix;
        useCustomMatrix = true;
    }

    // Reset to TRS mode
    void clearCustomMatrix() {
        useCustomMatrix = false;
        customMatrix = glm::mat4(1.0f);
    }

    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f}; // Euler angles in degrees
    glm::vec3 scale{1.0f};

private:
    glm::mat4 customMatrix{1.0f};
    bool useCustomMatrix = false;
};
