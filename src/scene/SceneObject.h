#pragma once

#include "../mesh/Mesh.h"
#include "Transform.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

// GPU resources for a single material (texture + descriptor set)
struct MaterialResources {
    VkImage textureImage = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
    VkImageView textureImageView = VK_NULL_HANDLE;
    uint32_t mipLevels = 1;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;  // Descriptor set for this material
};

// A renderable entity: mesh geometry + spatial transform + materials
struct SceneObject {
    std::string name = "Unnamed Object";  // Display name in Scene Hierarchy
    Mesh mesh;
    Transform transform;

    // Material resources (textures + descriptor sets)
    std::vector<MaterialResources> materialResources;  // One per material in mesh
    bool useMultiMaterial = false;  // Flag: true if mesh has multiple materials

    // Material index for centralized material buffer (PBR shaders)
    int materialIndex = 0;  // Index into MaterialManager's buffer (default = 0 = fallback material)

    // Cleanup GPU resources owned by this object
    void cleanupMaterialResources(VkDevice device) {
        for (auto& matRes : materialResources) {
            // Only cleanup resources we own (not fallback references)
            if (matRes.textureImageMemory != VK_NULL_HANDLE) {
                vkDestroyImageView(device, matRes.textureImageView, nullptr);
                vkDestroyImage(device, matRes.textureImage, nullptr);
                vkFreeMemory(device, matRes.textureImageMemory, nullptr);
            }
        }
        materialResources.clear();
    }
};
