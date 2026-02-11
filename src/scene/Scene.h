#pragma once

#include "SceneObject.h"
#include "Camera.h"
#include "Light.h"
#include <vector>

// Top-level container for all renderable state.
// Owns the camera, scene objects, and lights.
struct Scene {
    Camera camera;
    std::vector<SceneObject> objects;
    std::vector<Light> lights;

    // Cleanup all GPU resources owned by scene objects.
    void cleanup(VkDevice device) {
        for (auto& obj : objects) {
            obj.mesh.cleanup(device);
            obj.cleanupMaterialResources(device);
        }
    }
};
