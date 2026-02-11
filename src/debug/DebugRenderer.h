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
                const class Scene& scene,
                bool drawGrid, bool drawAxes, bool drawLightGizmos);

    void cleanup(VkDevice device);

    // Update swapchain extent and rebuild pipelines (for window resize)
    void updateExtent(VkExtent2D newExtent);

    // Rebuild light gizmo geometry from scene lights (call before record)
    void updateLightGizmos(const class Scene& scene, VkDevice device,
                           class VulkanResource& resource, class VulkanCommand& command);

private:
    void buildDebugGeometry();
    void buildLightGizmoGeometry(const class Scene& scene);
    void createDebugPipeline(VkDevice device, ShaderManager& shaderManager,
                             VkExtent2D extent, VkDescriptorSetLayout layout,
                             VkRenderPass renderPass);
    void createGridPipeline(VkDevice device, ShaderManager& shaderManager,
                            VkExtent2D extent, VkDescriptorSetLayout layout,
                            VkRenderPass renderPass);

    Mesh debugMesh;
    Mesh lightGizmoMesh;

    VkPipeline debugPipeline = VK_NULL_HANDLE;
    VkPipelineLayout debugPipelineLayout = VK_NULL_HANDLE;

    VkPipeline gridPipeline = VK_NULL_HANDLE;
    VkPipelineLayout gridPipelineLayout = VK_NULL_HANDLE;

    // Cached init parameters for pipeline rebuilding
    VkDevice device = VK_NULL_HANDLE;
    ShaderManager* shaderManager = nullptr;
    VkExtent2D swapchainExtent{};
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
};
