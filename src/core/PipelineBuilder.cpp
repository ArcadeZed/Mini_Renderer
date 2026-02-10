#include "PipelineBuilder.h"
#include "ShaderManager.h"
#include <stdexcept>
#include <iostream>

PipelineBuilder::PipelineBuilder(VkDevice device) : device(device) {}

PipelineBuilder& PipelineBuilder::setShaders(ShaderManager& shaderManager,
                                              const std::string& vertPath, const std::string& fragPath) {
    auto stages = shaderManager.loadVertexFragmentStages(vertPath, fragPath);
    shaderStages.assign(stages.begin(), stages.end());
    shadersSet = true;
    return *this;
}

PipelineBuilder& PipelineBuilder::setShaders(ShaderManager& shaderManager,
                                              const std::string& vertPath, const std::string& geomPath,
                                              const std::string& fragPath) {
    auto stages = shaderManager.loadVertexGeometryFragmentStages(vertPath, geomPath, fragPath);
    shaderStages.assign(stages.begin(), stages.end());
    shadersSet = true;
    return *this;
}

PipelineBuilder& PipelineBuilder::setVertexInput(VkVertexInputBindingDescription binding,
                                                   const VkVertexInputAttributeDescription* attributes,
                                                   uint32_t attributeCount) {
    vertexBinding = binding;
    vertexAttributes.assign(attributes, attributes + attributeCount);
    hasVertexInput = true;
    return *this;
}

PipelineBuilder& PipelineBuilder::setTopology(VkPrimitiveTopology t) {
    topology = t;
    return *this;
}

PipelineBuilder& PipelineBuilder::setViewport(VkExtent2D extent) {
    viewportExtent = extent;
    return *this;
}

PipelineBuilder& PipelineBuilder::setCullMode(VkCullModeFlags mode) {
    cullMode = mode;
    return *this;
}

PipelineBuilder& PipelineBuilder::setPolygonMode(VkPolygonMode mode) {
    polygonMode = mode;
    return *this;
}

PipelineBuilder& PipelineBuilder::setDepthTest(bool enable, bool write, VkCompareOp op) {
    depthTestEnable = enable;
    depthWriteEnable = write;
    depthCompareOp = op;
    return *this;
}

PipelineBuilder& PipelineBuilder::setBlending(bool enable) {
    blendingEnable = enable;
    return *this;
}

PipelineBuilder& PipelineBuilder::setDescriptorLayouts(const std::vector<VkDescriptorSetLayout>& layouts) {
    descriptorLayouts = layouts;
    return *this;
}

PipelineBuilder& PipelineBuilder::setPushConstants(const std::vector<VkPushConstantRange>& ranges) {
    pushConstantRanges = ranges;
    return *this;
}

PipelineBuilder& PipelineBuilder::setRenderPass(VkRenderPass rp, uint32_t sp) {
    renderPass = rp;
    subpass = sp;
    return *this;
}

PipelineBuildResult PipelineBuilder::build() {
    if (!shadersSet) {
        throw std::runtime_error("PipelineBuilder: shaders not set!");
    }
    if (renderPass == VK_NULL_HANDLE) {
        throw std::runtime_error("PipelineBuilder: render pass not set!");
    }
    if (viewportExtent.width == 0 || viewportExtent.height == 0) {
        throw std::runtime_error("PipelineBuilder: viewport extent not set!");
    }

    // Vertex input
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    if (hasVertexInput) {
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &vertexBinding;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size());
        vertexInputInfo.pVertexAttributeDescriptions = vertexAttributes.data();
    } else {
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr;
    }

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = topology;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport + scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(viewportExtent.width);
    viewport.height = static_cast<float>(viewportExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = viewportExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = polygonMode;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = cullMode;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = depthTestEnable ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = depthWriteEnable ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = depthCompareOp;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (blendingEnable) {
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    } else {
        colorBlendAttachment.blendEnable = VK_FALSE;
    }

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorLayouts.empty() ? nullptr : descriptorLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
    pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.empty() ? nullptr : pushConstantRanges.data();

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("PipelineBuilder: failed to create pipeline layout!");
    }

    // Graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = subpass;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        throw std::runtime_error("PipelineBuilder: failed to create graphics pipeline!");
    }

    return {pipeline, pipelineLayout};
}
