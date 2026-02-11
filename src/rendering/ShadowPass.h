#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

class ShaderManager;
class VulkanResource;
class VulkanCommand;
class Scene;
struct Light;

// Shadow pass: Renders scene from light's perspective to generate depth map
class ShadowPass {
public:
    ShadowPass() = default;
    ~ShadowPass() = default;

    ShadowPass(const ShadowPass&) = delete;
    ShadowPass& operator=(const ShadowPass&) = delete;

    void init(VkDevice device,
              VulkanResource& resource,
              VulkanCommand& command,
              ShaderManager& shaderManager,
              uint32_t shadowMapSize = 1024);

    void record(VkCommandBuffer cmd,
                const Scene& scene,
                const Light& light);  // Light that casts shadows

    void cleanup(VkDevice device);

    // Getters for shadow map resources (for main pass to sample)
    VkImageView getShadowMapView() const { return shadowMapView; }
    VkSampler getShadowMapSampler() const { return shadowMapSampler; }

private:
    void createShadowResources();
    void createShadowRenderPass();
    void createShadowFramebuffer();
    void createShadowPipeline();
    void createShadowSampler();

    VkDevice device = VK_NULL_HANDLE;
    VulkanResource* resource = nullptr;
    VulkanCommand* command = nullptr;
    ShaderManager* shaderManager = nullptr;

    uint32_t shadowMapSize = 1024;

    // Shadow map resources
    VkImage shadowMapImage = VK_NULL_HANDLE;
    VkDeviceMemory shadowMapMemory = VK_NULL_HANDLE;
    VkImageView shadowMapView = VK_NULL_HANDLE;
    VkSampler shadowMapSampler = VK_NULL_HANDLE;

    // Shadow render pass and framebuffer
    VkRenderPass shadowRenderPass = VK_NULL_HANDLE;
    VkFramebuffer shadowFramebuffer = VK_NULL_HANDLE;

    // Shadow pipeline
    VkPipeline shadowPipeline = VK_NULL_HANDLE;
    VkPipelineLayout shadowPipelineLayout = VK_NULL_HANDLE;
};
