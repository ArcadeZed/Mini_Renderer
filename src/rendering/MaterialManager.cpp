#include "MaterialManager.h"
#include "../core/VulkanContext.h"
#include "../core/VulkanCommand.h"
#include "../core/VulkanResource.h"
#include "../mesh/GLTFLoader.h"
#include <stdexcept>
#include <cstring>
#include <iostream>

MaterialManager::MaterialManager(VulkanContext* context, VulkanCommand* command, VulkanResource* resource)
    : m_context(context), m_command(command), m_resource(resource) {
}

MaterialManager::~MaterialManager() {
    cleanup();
}

void MaterialManager::addDefaultMaterial() {
    GPUMaterial defaultMat{};

    // White diffuse color with full opacity
    defaultMat.baseColorFactor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

    // Metallic=0, Roughness=0.5 (medium), NormalScale=1.0, AlphaMode=0 (Opaque)
    defaultMat.metallicRoughnessEmissive = glm::vec4(0.0f, 0.5f, 1.0f, 0.0f);

    // No emissive
    defaultMat.emissiveFactor = glm::vec3(0.0f);
    defaultMat.alphaCutoff = 0.5f;  // Default alpha cutoff

    // BaseColor uses white texture at index 0, others use -1 (no texture)
    defaultMat.textureIndices = glm::ivec4(0, -1, -1, -1);

    m_materials.push_back(defaultMat);
}

int MaterialManager::addMaterial(const GLTFMaterial& gltfMaterial, int textureIndexOffset) {
    GPUMaterial gpuMat{};

    // Base color
    gpuMat.baseColorFactor = gltfMaterial.baseColorFactor;

    // Metallic, Roughness, Normal Scale, Alpha Mode in one vec4
    gpuMat.metallicRoughnessEmissive.x = gltfMaterial.metallicFactor;
    gpuMat.metallicRoughnessEmissive.y = gltfMaterial.roughnessFactor;
    gpuMat.metallicRoughnessEmissive.z = gltfMaterial.normalScale;
    gpuMat.metallicRoughnessEmissive.w = static_cast<float>(gltfMaterial.alphaMode);  // 0=Opaque, 1=Mask, 2=Blend

    // Emissive factor
    gpuMat.emissiveFactor = gltfMaterial.emissiveFactor;
    gpuMat.alphaCutoff = gltfMaterial.alphaCutoff;  // For Alpha_Mask mode

    // Texture indices (keep -1 if no texture, shader will check >= 0)
    gpuMat.textureIndices.x = gltfMaterial.baseColorTextureIndex >= 0
        ? gltfMaterial.baseColorTextureIndex + textureIndexOffset
        : -1; // no texture

    gpuMat.textureIndices.y = gltfMaterial.normalTextureIndex >= 0
        ? gltfMaterial.normalTextureIndex + textureIndexOffset
        : -1; // no texture (shader will use vertex normal)

    gpuMat.textureIndices.z = gltfMaterial.metallicRoughnessTextureIndex >= 0
        ? gltfMaterial.metallicRoughnessTextureIndex + textureIndexOffset
        : -1; // no texture (shader will use factor values only)

    gpuMat.textureIndices.w = gltfMaterial.emissiveTextureIndex >= 0
        ? gltfMaterial.emissiveTextureIndex + textureIndexOffset
        : -1; // no texture (no emission)

    m_materials.push_back(gpuMat);
    return static_cast<int>(m_materials.size() - 1);
}

void MaterialManager::uploadToGPU() {
    if (m_materials.empty()) {
        throw std::runtime_error("MaterialManager: No materials to upload!");
    }

    // Cleanup old buffer if it exists (allow re-upload)
    if (m_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_context->getDevice(), m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
    }
    if (m_bufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_context->getDevice(), m_bufferMemory, nullptr);
        m_bufferMemory = VK_NULL_HANDLE;
    }

    VkDeviceSize bufferSize = m_materials.size() * sizeof(GPUMaterial);

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    m_resource->createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingMemory
    );

    // Copy material data to staging buffer
    void* data;
    vkMapMemory(m_context->getDevice(), stagingMemory, 0, bufferSize, 0, &data);
    std::memcpy(data, m_materials.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(m_context->getDevice(), stagingMemory);

    // Create device-local buffer (GPU-only)
    m_resource->createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_buffer,
        m_bufferMemory
    );

    // Copy staging → device-local
    VkCommandBuffer cmd = m_command->beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(cmd, stagingBuffer, m_buffer, 1, &copyRegion);

    m_command->endSingleTimeCommands(cmd);

    // Cleanup staging buffer
    vkDestroyBuffer(m_context->getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_context->getDevice(), stagingMemory, nullptr);

    m_isUploaded = true;
}

void MaterialManager::cleanup() {
    if (m_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_context->getDevice(), m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
    }

    if (m_bufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_context->getDevice(), m_bufferMemory, nullptr);
        m_bufferMemory = VK_NULL_HANDLE;
    }

    m_isUploaded = false;
}
