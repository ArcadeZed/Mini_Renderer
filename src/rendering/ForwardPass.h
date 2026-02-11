#pragma once

#include <vulkan/vulkan.h>
#include <vector>

class ShaderManager;
class Scene;
class ILightingModel;
class MaterialManager;

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
              VkDescriptorSetLayout pbrDescriptorSetLayout,  // Set 1: Material SSBO + Texture Array
              VkRenderPass renderPass,
              MaterialManager* materialManager = nullptr);  // For alpha mode queries

    void record(VkCommandBuffer cmd,
                VkDescriptorSet globalDescriptorSet,
                VkDescriptorSet pbrDescriptorSet,  // Set 1 for PBR shader
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
    MaterialManager* materialManager = nullptr;  // For querying material alpha modes
    VkExtent2D swapchainExtent{};
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;  // Set 0: Global UBO + Fallback Texture
    VkDescriptorSetLayout pbrDescriptorSetLayout = VK_NULL_HANDLE;  // Set 1: Material SSBO + Texture Array
    VkRenderPass renderPass = VK_NULL_HANDLE;

    ILightingModel* currentModel = nullptr;

    VkPipeline opaquePipeline = VK_NULL_HANDLE;      // For opaque objects
    VkPipeline blendedPipeline = VK_NULL_HANDLE;     // For alpha-blended objects
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
};
