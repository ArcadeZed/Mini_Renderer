#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <array>

class ShaderManager;

// Builder pattern for VkGraphicsPipeline creation.
// Eliminates duplicated boilerplate across multiple pipeline functions.
// Usage:
//   auto [pipeline, layout] = PipelineBuilder(device)
//       .setShaders(shaderManager, vertPath, fragPath)
//       .setVertexInput(binding, attributes)
//       .setRenderPass(renderPass)
//       .setDescriptorLayouts({layout})
//       .build();
struct PipelineBuildResult {
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

class PipelineBuilder {
public:
    explicit PipelineBuilder(VkDevice device);

    // Required: shader stages (vertex + fragment)
    PipelineBuilder& setShaders(ShaderManager& shaderManager,
                                const std::string& vertPath, const std::string& fragPath);

    // Required: shader stages (vertex + geometry + fragment)
    PipelineBuilder& setShaders(ShaderManager& shaderManager,
                                const std::string& vertPath, const std::string& geomPath,
                                const std::string& fragPath);

    // Vertex input (default: no vertex input)
    PipelineBuilder& setVertexInput(VkVertexInputBindingDescription binding,
                                     const VkVertexInputAttributeDescription* attributes,
                                     uint32_t attributeCount);

    // Topology (default: TRIANGLE_LIST)
    PipelineBuilder& setTopology(VkPrimitiveTopology topology);

    // Viewport + scissor from swapchain extent
    PipelineBuilder& setViewport(VkExtent2D extent);

    // Cull mode (default: BACK_BIT)
    PipelineBuilder& setCullMode(VkCullModeFlags cullMode);

    // Polygon mode (default: FILL)
    PipelineBuilder& setPolygonMode(VkPolygonMode mode);

    // Depth test (default: enabled, write, LESS)
    PipelineBuilder& setDepthTest(bool enable, bool write, VkCompareOp compareOp);

    // Alpha blending (default: disabled)
    PipelineBuilder& setBlending(bool enable);

    // Descriptor set layouts
    PipelineBuilder& setDescriptorLayouts(const std::vector<VkDescriptorSetLayout>& layouts);

    // Push constant ranges (default: none)
    PipelineBuilder& setPushConstants(const std::vector<VkPushConstantRange>& ranges);

    // Render pass + subpass (default: subpass 0)
    PipelineBuilder& setRenderPass(VkRenderPass renderPass, uint32_t subpass = 0);

    // Build pipeline + layout. Caller owns both and must destroy them.
    PipelineBuildResult build();

private:
    VkDevice device;

    // Shader stages (set by setShaders) - flexible for 2 or 3 stages
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    bool shadersSet = false;

    // Vertex input
    VkVertexInputBindingDescription vertexBinding{};
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;
    bool hasVertexInput = false;

    // Fixed-function state with defaults
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkExtent2D viewportExtent{};
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    bool depthTestEnable = true;
    bool depthWriteEnable = true;
    VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
    bool blendingEnable = false;

    // Layout
    std::vector<VkDescriptorSetLayout> descriptorLayouts;
    std::vector<VkPushConstantRange> pushConstantRanges;

    // Render pass
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t subpass = 0;
};
