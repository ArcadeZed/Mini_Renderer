#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

// Light types
enum class LightType : int {
    POINT = 0,       // Point light (position, radiates in all directions)
    DIRECTIONAL = 1, // Directional light (no position, only direction - like sun)
    SPOT = 2         // Spot light (position + direction + cone angle) - optional for later
};

// CPU-side light representation (scene management)
struct Light {
    std::string name = "Light";
    LightType type = LightType::POINT;

    // Spatial properties
    glm::vec3 position{0.0f, 10.0f, 0.0f};  // World position (for Point/Spot lights)
    glm::vec3 direction{0.0f, -1.0f, 0.0f}; // Direction (for Directional/Spot lights)

    // Color and intensity
    glm::vec3 color{1.0f, 1.0f, 1.0f};  // RGB color
    float intensity = 1.0f;              // Brightness multiplier

    // Attenuation (for Point/Spot lights, inverse-square law with adjustments)
    float attenuationConstant = 1.0f;   // Constant term
    float attenuationLinear = 0.09f;    // Linear term
    float attenuationQuadratic = 0.032f; // Quadratic term
    float range = 50.0f;                 // Maximum effective range

    // Spot light properties (for future use)
    float spotInnerCutoff = 12.5f;  // Inner cone angle (degrees)
    float spotOuterCutoff = 17.5f;  // Outer cone angle (degrees)

    // Shadow properties
    bool castsShadows = true;          // Enable/disable shadow casting for this light
    float shadowBias = 0.005f;         // Depth bias to prevent shadow acne
    int shadowMapResolution = 4096;    // Shadow map resolution
    float shadowNearPlane = 0.1f;      // Near plane for shadow frustum
    float shadowFarPlane = 200.0f;     // Far plane for shadow frustum
    float shadowOrthoSize = 50.0f;     // Orthographic size for directional light shadows (~scene radius)

    // State
    bool enabled = true;  // Can be toggled in UI

    // Helper: Get attenuation factor at a given distance
    float getAttenuationAt(float distance) const {
        if (type == LightType::DIRECTIONAL) {
            return 1.0f;  // No attenuation for directional lights
        }
        if (distance > range) {
            return 0.0f;  // Beyond range
        }
        return 1.0f / (attenuationConstant + attenuationLinear * distance + attenuationQuadratic * distance * distance);
    }

    // Helper: Compute light space view-projection matrix for directional lights
    glm::mat4 getLightSpaceMatrix(const glm::vec3& sceneCenter = glm::vec3(0.0f)) const {
        if (type == LightType::DIRECTIONAL) {
            // Position light far away in the opposite direction
            glm::vec3 lightPos = sceneCenter - glm::normalize(direction) * shadowFarPlane * 0.5f;

            // Choose up vector that is not parallel to light direction
            // If light points straight up/down, use X-axis as up
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
            glm::vec3 lightDir = glm::normalize(direction);
            if (glm::abs(glm::dot(lightDir, up)) > 0.99f) {
                up = glm::vec3(1.0f, 0.0f, 0.0f);  // Use X-axis instead
            }

            // Look at scene center
            glm::mat4 lightView = glm::lookAt(lightPos, sceneCenter, up);

            // Orthographic projection for directional light
            glm::mat4 lightProj = glm::orthoRH_ZO(
                -shadowOrthoSize, shadowOrthoSize,
                -shadowOrthoSize, shadowOrthoSize,
                shadowNearPlane, shadowFarPlane
            );

            return lightProj * lightView;
        }
        // TODO: Point light shadow (cube map) and Spot light shadow (perspective projection)
        return glm::mat4(1.0f);
    }
};

// GPU-aligned light data (matches shader layout)
// IMPORTANT: Must match std140 alignment in shaders (16-byte boundaries)
struct GPULight {
    glm::vec4 positionAndType;   // xyz = position, w = type (0/1/2)
    glm::vec4 colorAndIntensity; // rgb = color, a = intensity
    glm::vec4 directionAndRange; // xyz = direction (normalized), w = range
    glm::vec4 attenuation;       // x = constant, y = linear, z = quadratic, w = unused (padding)

    // Constructor from CPU Light
    GPULight() = default;

    explicit GPULight(const Light& light) {
        positionAndType = glm::vec4(light.position, static_cast<float>(light.type));
        colorAndIntensity = glm::vec4(light.color, light.intensity);
        directionAndRange = glm::vec4(glm::normalize(light.direction), light.range);
        attenuation = glm::vec4(light.attenuationConstant, light.attenuationLinear, light.attenuationQuadratic, 0.0f);
    }
};
