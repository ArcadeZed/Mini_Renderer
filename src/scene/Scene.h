#pragma once

#include "SceneObject.h"
#include "Camera.h"
#include <vector>

// Top-level container for all renderable state.
// Owns the camera, scene objects, and (later) lights.
struct Scene {
    Camera camera;
    std::vector<SceneObject> objects;

    // Cleanup all GPU resources owned by scene objects.
    void cleanup(VkDevice device) {
        for (auto& obj : objects) {
            obj.mesh.cleanup(device);
            obj.cleanupMaterialResources(device);
        }
    }
};
