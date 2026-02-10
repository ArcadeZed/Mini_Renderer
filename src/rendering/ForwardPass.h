#pragma once

#include <vulkan/vulkan.h>
#include <vector>

class ShaderManager;
class Scene;
class ILightingModel;

class ForwardPass {
public:
    ForwardPass() = default;
    ~ForwardPass() = default;

    ForwardPass(const ForwardPass&) = delete;
    ForwardPass& operator=(const ForwardPass&) = delete;

    void init(VkDevice device,
              ShaderManager& shaderManager,
              VkExtent2D swapchainExtent,
              VkDescriptorSetLayout descriptorSetLayout,
              VkRenderPass renderPass);

    void record(VkCommandBuffer cmd,
                VkDescriptorSet globalDescriptorSet,
                const Scene& scene);

    void cleanup(VkDevice device);

    // Set the lighting model and rebuild the pipeline
    void setLightingModel(ILightingModel* model);

    // Update swapchain extent and rebuild pipeline (for window resize)
    void updateExtent(VkExtent2D newExtent);

private:
    void buildPipeline();

    VkDevice device = VK_NULL_HANDLE;
    ShaderManager* shaderManager = nullptr;
    VkExtent2D swapchainExtent{};
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;

    ILightingModel* currentModel = nullptr;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
};
