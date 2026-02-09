#pragma once

#include <vulkan/vulkan.h>
#include "../mesh/Mesh.h"

class ShaderManager;
class VulkanResource;
class VulkanCommand;

class DebugRenderer {
public:
    DebugRenderer() = default;
    ~DebugRenderer() = default;

    DebugRenderer(const DebugRenderer&) = delete;
    DebugRenderer& operator=(const DebugRenderer&) = delete;

    void init(VkDevice device,
              ShaderManager& shaderManager,
              VkExtent2D swapchainExtent,
              VkDescriptorSetLayout descriptorSetLayout,
              VkRenderPass renderPass,
              VulkanResource& resource,
              VulkanCommand& command);

    void record(VkCommandBuffer cmd, VkDescriptorSet descriptorSet,
                bool drawGrid, bool drawAxes);

    void cleanup(VkDevice device);

private:
    void buildDebugGeometry();
    void createDebugPipeline(VkDevice device, ShaderManager& shaderManager,
                             VkExtent2D extent, VkDescriptorSetLayout layout,
                             VkRenderPass renderPass);
    void createGridPipeline(VkDevice device, ShaderManager& shaderManager,
                            VkExtent2D extent, VkDescriptorSetLayout layout,
                            VkRenderPass renderPass);

    Mesh debugMesh;

    VkPipeline debugPipeline = VK_NULL_HANDLE;
    VkPipelineLayout debugPipelineLayout = VK_NULL_HANDLE;

    VkPipeline gridPipeline = VK_NULL_HANDLE;
    VkPipelineLayout gridPipelineLayout = VK_NULL_HANDLE;
};
