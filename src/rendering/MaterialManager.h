#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>

class VulkanContext;
class VulkanCommand;
class VulkanResource;
struct GLTFMaterial;

// GPU-aligned material data (matches shader layout)
// IMPORTANT: Must match std140/std430 layout in shaders
struct GPUMaterial {
    // PBR Base (16 bytes alignment)
    glm::vec4 baseColorFactor;          // RGB + Alpha (16 bytes)

    glm::vec4 metallicRoughnessEmissive; // Metallic, Roughness, NormalScale, AlphaMode (16 bytes)
    glm::vec3 emissiveFactor;           // RGB (12 bytes)
    float alphaCutoff;                  // Alpha cutoff for Alpha_Mask mode (4 bytes)

    // Texture indices (int = 4 bytes each, pack into vec4 for alignment)
    glm::ivec4 textureIndices;          // baseColor, normal, metallicRoughness, emissive (16 bytes)

    // Total: 64 bytes (power of 2, good for GPU)
};

class MaterialManager {
public:
    MaterialManager(VulkanContext* context, VulkanCommand* command, VulkanResource* resource);
    ~MaterialManager();

    // Add a default fallback material (white diffuse, medium roughness) at index 0
    // Should be called FIRST during initialization
    void addDefaultMaterial();

    // Add a material from glTF data and return its index
    // textureIndexOffset: Offset to add to texture indices (e.g., if TextureManager has default white at 0)
    int addMaterial(const GLTFMaterial& gltfMaterial, int textureIndexOffset = 0);

    // Upload all materials to GPU as a storage buffer
    void uploadToGPU();

    // Get GPU buffer handle (for descriptor binding)
    VkBuffer getBuffer() const { return m_buffer; }
    VkDeviceSize getBufferSize() const { return m_materials.size() * sizeof(GPUMaterial); }

    // Get material count
    size_t getMaterialCount() const { return m_materials.size(); }

    // Get alpha mode for a material (0=Opaque, 1=Mask, 2=Blend)
    int getAlphaMode(int materialIndex) const {
        if (materialIndex < 0 || materialIndex >= static_cast<int>(m_materials.size())) {
            return 0;  // Default to Opaque if out of bounds
        }
        return static_cast<int>(m_materials[materialIndex].metallicRoughnessEmissive.w);
    }

    // Cleanup
    void cleanup();

private:
    VulkanContext* m_context;
    VulkanCommand* m_command;
    VulkanResource* m_resource;

    std::vector<GPUMaterial> m_materials;

    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_bufferMemory = VK_NULL_HANDLE;
    bool m_isUploaded = false;
};
