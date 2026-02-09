#pragma once

#include <vulkan/vulkan.h>

class ShaderManager;
class Scene;

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
                VkDescriptorSet descriptorSet,
                const Scene& scene);

    void cleanup(VkDevice device);

private:
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
};
