#include "ShadowPass.h"
#include "../core/VulkanResource.h"
#include "../core/VulkanCommand.h"
#include "../core/ShaderManager.h"
#include "../scene/Scene.h"
#include "../scene/Light.h"
#include "../mesh/Mesh.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <stdexcept>

// Push constant for shadow pass (just light space matrix + model)
struct ShadowPushConstant {
    glm::mat4 lightSpaceMatrix;
    glm::mat4 model;
};

void ShadowPass::init(VkDevice dev,
                      VulkanResource& res,
                      VulkanCommand& cmd,
                      ShaderManager& shaderMgr,
                      uint32_t shadowSize) {
    device = dev;
    resource = &res;
    command = &cmd;
    shaderManager = &shaderMgr;
    shadowMapSize = shadowSize;

    createShadowResources();
    createShadowRenderPass();
    createShadowFramebuffer();
    createShadowPipeline();
    createShadowSampler();

    // Cube shadow map (point lights)
    createCubeShadowResources();
    createCubeShadowFramebuffers();
    createCubeShadowPipeline();
    createCubeShadowSampler();
    createDummyCubeTexture();

    std::cout << "ShadowPass initialized (" << shadowMapSize << "x" << shadowMapSize
              << ", cube " << cubeMapSize << "x" << cubeMapSize << ")" << std::endl;
}

void ShadowPass::createShadowResources() {
    // Create depth image for shadow map using VulkanResource helper
    resource->createImage(shadowMapSize, shadowMapSize, 1,
                         VK_FORMAT_D32_SFLOAT,
                         VK_IMAGE_TILING_OPTIMAL,
                         VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         shadowMapImage, shadowMapMemory);

    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = shadowMapImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_D32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &shadowMapView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow map image view!");
    }

    // Create grayscale preview view (R→RGB swizzle for ImGui display)
    VkImageViewCreateInfo previewViewInfo = viewInfo;
    previewViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    previewViewInfo.components.g = VK_COMPONENT_SWIZZLE_R;
    previewViewInfo.components.b = VK_COMPONENT_SWIZZLE_R;
    previewViewInfo.components.a = VK_COMPONENT_SWIZZLE_ONE;

    if (vkCreateImageView(device, &previewViewInfo, nullptr, &shadowMapPreviewView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow map preview view!");
    }

    // Note: No initial layout transition needed - the render pass will handle it.
    // Image starts in UNDEFINED, render pass transitions to DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    // then to DEPTH_STENCIL_READ_ONLY_OPTIMAL for sampling in the main pass.

    std::cout << "Shadow map resources created." << std::endl;
}

void ShadowPass::createShadowRenderPass() {
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 0;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;  // No color attachments
    subpass.pDepthStencilAttachment = &depthRef;

    // Two dependencies for proper synchronization:
    // 1. Before shadow pass: Wait for previous frame's fragment shader to finish reading
    VkSubpassDependency dependencies[2] = {};

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // 2. After shadow pass: Transition to READ_ONLY for main pass
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &depthAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 2;
    renderPassInfo.pDependencies = dependencies;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &shadowRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow render pass!");
    }

    std::cout << "Shadow render pass created." << std::endl;
}

void ShadowPass::createShadowFramebuffer() {
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = shadowRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &shadowMapView;
    framebufferInfo.width = shadowMapSize;
    framebufferInfo.height = shadowMapSize;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &shadowFramebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow framebuffer!");
    }

    std::cout << "Shadow framebuffer created." << std::endl;
}

void ShadowPass::createShadowPipeline() {
    // Load shadow shader (only vertex shader, no fragment shader needed)
    VkShaderModule vertShader = shaderManager->loadShader(
        std::string(SHADER_DIR) + "/shadow.vert.spv"
    );

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertShader;
    vertStage.pName = "main";

    // Vertex input (same as main pass)
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(shadowMapSize);
    viewport.height = static_cast<float>(shadowMapSize);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {shadowMapSize, shadowMapSize};

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
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;  // Cull front faces → write back faces to shadow map (reduces acne + light leaking)
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_TRUE;  // Enable depth bias (set dynamically in record())
    rasterizer.depthBiasConstantFactor = 0.0f;  // Will be set via vkCmdSetDepthBias
    rasterizer.depthBiasSlopeFactor = 0.0f;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth and stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Push constant for lightSpaceMatrix + model
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(ShadowPushConstant);

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;  // No descriptor sets
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &shadowPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow pipeline layout!");
    }

    // Dynamic state: depth bias (set per-frame via vkCmdSetDepthBias)
    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_DEPTH_BIAS };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 1;
    dynamicState.pDynamicStates = dynamicStates;

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 1;  // Only vertex shader
    pipelineInfo.pStages = &vertStage;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = nullptr;  // No color attachments
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = shadowPipelineLayout;
    pipelineInfo.renderPass = shadowRenderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &shadowPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow pipeline!");
    }

    vkDestroyShaderModule(device, vertShader, nullptr);

    std::cout << "Shadow pipeline created." << std::endl;
}

void ShadowPass::createShadowSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;  // Outside shadow map = no shadow
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;  // Manual comparison in fragment shader
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &shadowMapSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow map sampler!");
    }

    std::cout << "Shadow map sampler created." << std::endl;
}

// ============================================================================
// CUBE SHADOW MAP (Point Lights)
// ============================================================================

void ShadowPass::createCubeShadowResources() {
    // Create cube map image (6 layers, D32_SFLOAT)
    resource->createImage(cubeMapSize, cubeMapSize, 1,
                         VK_FORMAT_D32_SFLOAT,
                         VK_IMAGE_TILING_OPTIMAL,
                         VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         cubeShadowMapImage, cubeShadowMapMemory,
                         6, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

    // Create CUBE image view (for sampling in main pass fragment shader)
    VkImageViewCreateInfo cubeViewInfo{};
    cubeViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    cubeViewInfo.image = cubeShadowMapImage;
    cubeViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    cubeViewInfo.format = VK_FORMAT_D32_SFLOAT;
    cubeViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    cubeViewInfo.subresourceRange.baseMipLevel = 0;
    cubeViewInfo.subresourceRange.levelCount = 1;
    cubeViewInfo.subresourceRange.baseArrayLayer = 0;
    cubeViewInfo.subresourceRange.layerCount = 6;

    if (vkCreateImageView(device, &cubeViewInfo, nullptr, &cubeShadowMapView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create cube shadow map view!");
    }

    // Create per-face 2D views (for framebuffer attachments) + preview views (ImGui)
    for (int face = 0; face < 6; face++) {
        VkImageViewCreateInfo faceViewInfo{};
        faceViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        faceViewInfo.image = cubeShadowMapImage;
        faceViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        faceViewInfo.format = VK_FORMAT_D32_SFLOAT;
        faceViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        faceViewInfo.subresourceRange.baseMipLevel = 0;
        faceViewInfo.subresourceRange.levelCount = 1;
        faceViewInfo.subresourceRange.baseArrayLayer = static_cast<uint32_t>(face);
        faceViewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &faceViewInfo, nullptr, &cubeFaceViews[face]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create cube face view!");
        }

        // Preview view with R→RGB swizzle for grayscale ImGui display
        VkImageViewCreateInfo previewInfo = faceViewInfo;
        previewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        previewInfo.components.g = VK_COMPONENT_SWIZZLE_R;
        previewInfo.components.b = VK_COMPONENT_SWIZZLE_R;
        previewInfo.components.a = VK_COMPONENT_SWIZZLE_ONE;

        if (vkCreateImageView(device, &previewInfo, nullptr, &cubeFacePreviewViews[face]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create cube face preview view!");
        }
    }

    std::cout << "Cube shadow map resources created (" << cubeMapSize << "x" << cubeMapSize << " x6)." << std::endl;
}

void ShadowPass::createCubeShadowFramebuffers() {
    for (int face = 0; face < 6; face++) {
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = shadowRenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &cubeFaceViews[face];
        fbInfo.width = cubeMapSize;
        fbInfo.height = cubeMapSize;
        fbInfo.layers = 1;

        if (vkCreateFramebuffer(device, &fbInfo, nullptr, &cubeFaceFramebuffers[face]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create cube face framebuffer!");
        }
    }

    std::cout << "Cube shadow framebuffers created (6 faces)." << std::endl;
}

void ShadowPass::createCubeShadowPipeline() {
    // Load cube shadow shaders (vertex + fragment for linear depth)
    VkShaderModule vertShader = shaderManager->loadShader(
        std::string(SHADER_DIR) + "/shadow_cube.vert.spv"
    );
    VkShaderModule fragShader = shaderManager->loadShader(
        std::string(SHADER_DIR) + "/shadow_cube.frag.spv"
    );

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertShader;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragShader;
    stages[1].pName = "main";

    // Vertex input (same as main pass)
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(cubeMapSize);
    viewport.height = static_cast<float>(cubeMapSize);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {cubeMapSize, cubeMapSize};

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_TRUE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Push constants: faceVP + model + lightPosAndFarPlane = 144 bytes
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(CubeShadowPushConstant);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &cubeShadowPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create cube shadow pipeline layout!");
    }

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_DEPTH_BIAS };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 1;
    dynamicState.pDynamicStates = dynamicStates;

    // Color blend state (no color attachments, but Vulkan requires a valid pointer)
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 0;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = cubeShadowPipelineLayout;
    pipelineInfo.renderPass = shadowRenderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &cubeShadowPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create cube shadow pipeline!");
    }

    vkDestroyShaderModule(device, vertShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);

    std::cout << "Cube shadow pipeline created." << std::endl;
}

void ShadowPass::createCubeShadowSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &cubeShadowMapSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create cube shadow map sampler!");
    }

    std::cout << "Cube shadow map sampler created." << std::endl;
}

void ShadowPass::createDummyCubeTexture() {
    // 1x1x6 cube texture, always bound when no point shadow is active
    resource->createImage(1, 1, 1,
                         VK_FORMAT_D32_SFLOAT,
                         VK_IMAGE_TILING_OPTIMAL,
                         VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         dummyCubeImage, dummyCubeMemory,
                         6, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

    // Transition to TRANSFER_DST, clear to 1.0 (max depth = no shadow), then to READ_ONLY
    VkCommandBuffer cmd = command->beginSingleTimeCommands();

    // Transition UNDEFINED → TRANSFER_DST
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = dummyCubeImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Clear to 1.0 (all faces)
    VkClearDepthStencilValue clearValue = { 1.0f, 0 };
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 6;

    vkCmdClearDepthStencilImage(cmd, dummyCubeImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &range);

    // Transition TRANSFER_DST → DEPTH_STENCIL_READ_ONLY
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    command->endSingleTimeCommands(cmd);

    // Create CUBE view for the dummy
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = dummyCubeImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format = VK_FORMAT_D32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6;

    if (vkCreateImageView(device, &viewInfo, nullptr, &dummyCubeView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create dummy cube view!");
    }

    std::cout << "Dummy cube texture created." << std::endl;
}

void ShadowPass::recordCube(VkCommandBuffer cmd,
                            const Scene& scene,
                            const Light& light) {
    glm::vec3 lightPos = light.position;
    float farPlane = light.shadowFarPlane;
    float nearPlane = light.shadowNearPlane;

    // 90° FOV perspective projection (1:1 aspect ratio)
    glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, nearPlane, farPlane);

    // 6 cube face directions and up vectors
    struct FaceInfo { glm::vec3 target; glm::vec3 up; };
    FaceInfo faces[6] = {
        { { 1, 0, 0}, {0,-1, 0} },  // +X
        { {-1, 0, 0}, {0,-1, 0} },  // -X
        { { 0, 1, 0}, {0, 0, 1} },  // +Y
        { { 0,-1, 0}, {0, 0,-1} },  // -Y
        { { 0, 0, 1}, {0,-1, 0} },  // +Z
        { { 0, 0,-1}, {0,-1, 0} },  // -Z
    };

    for (int face = 0; face < 6; face++) {
        glm::mat4 view = glm::lookAt(lightPos, lightPos + faces[face].target, faces[face].up);
        glm::mat4 faceVP = proj * view;

        // Begin render pass for this face
        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass = shadowRenderPass;
        rpInfo.framebuffer = cubeFaceFramebuffers[face];
        rpInfo.renderArea.offset = {0, 0};
        rpInfo.renderArea.extent = {cubeMapSize, cubeMapSize};

        VkClearValue clearVal{};
        clearVal.depthStencil = {1.0f, 0};
        rpInfo.clearValueCount = 1;
        rpInfo.pClearValues = &clearVal;

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cubeShadowPipeline);
        vkCmdSetDepthBias(cmd, light.shadowBias, 0.0f, light.shadowBias);

        // Render all scene objects
        for (const auto& obj : scene.objects) {
            if (!obj.mesh.isUploaded()) continue;

            CubeShadowPushConstant pushConst{};
            pushConst.faceVP = faceVP;
            pushConst.model = obj.transform.getModelMatrix();
            pushConst.lightPosAndFarPlane = glm::vec4(lightPos, farPlane);

            vkCmdPushConstants(cmd, cubeShadowPipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(CubeShadowPushConstant), &pushConst);

            obj.mesh.bind(cmd);
            obj.mesh.draw(cmd);
        }

        vkCmdEndRenderPass(cmd);
    }
}

// ============================================================================
// DIRECTIONAL SHADOW MAP
// ============================================================================

void ShadowPass::record(VkCommandBuffer cmd,
                        const Scene& scene,
                        const Light& light) {
    // Begin shadow render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = shadowRenderPass;
    renderPassInfo.framebuffer = shadowFramebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {shadowMapSize, shadowMapSize};

    VkClearValue clearValue{};
    clearValue.depthStencil = {1.0f, 0};  // Clear to max depth
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind shadow pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline);

    // Set depth bias (to prevent shadow acne)
    vkCmdSetDepthBias(cmd, light.shadowBias, 0.0f, light.shadowBias);

    // Get light space matrix
    glm::mat4 lightSpaceMatrix = light.getLightSpaceMatrix();

    // Render all scene objects
    for (const auto& obj : scene.objects) {
        if (!obj.mesh.isUploaded()) continue;

        // Push constants: lightSpaceMatrix + model
        ShadowPushConstant pushConst{};
        pushConst.lightSpaceMatrix = lightSpaceMatrix;
        pushConst.model = obj.transform.getModelMatrix();

        vkCmdPushConstants(cmd, shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(ShadowPushConstant), &pushConst);

        // Bind and draw mesh
        obj.mesh.bind(cmd);
        obj.mesh.draw(cmd);
    }

    vkCmdEndRenderPass(cmd);
}

void ShadowPass::cleanup(VkDevice dev) {
    // Cube shadow resources
    if (cubeShadowPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(dev, cubeShadowPipeline, nullptr);
        cubeShadowPipeline = VK_NULL_HANDLE;
    }
    if (cubeShadowPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(dev, cubeShadowPipelineLayout, nullptr);
        cubeShadowPipelineLayout = VK_NULL_HANDLE;
    }
    for (int i = 0; i < 6; i++) {
        if (cubeFaceFramebuffers[i] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(dev, cubeFaceFramebuffers[i], nullptr);
            cubeFaceFramebuffers[i] = VK_NULL_HANDLE;
        }
    }
    if (cubeShadowMapSampler != VK_NULL_HANDLE) {
        vkDestroySampler(dev, cubeShadowMapSampler, nullptr);
        cubeShadowMapSampler = VK_NULL_HANDLE;
    }
    for (int i = 0; i < 6; i++) {
        if (cubeFacePreviewViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(dev, cubeFacePreviewViews[i], nullptr);
            cubeFacePreviewViews[i] = VK_NULL_HANDLE;
        }
        if (cubeFaceViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(dev, cubeFaceViews[i], nullptr);
            cubeFaceViews[i] = VK_NULL_HANDLE;
        }
    }
    if (cubeShadowMapView != VK_NULL_HANDLE) {
        vkDestroyImageView(dev, cubeShadowMapView, nullptr);
        cubeShadowMapView = VK_NULL_HANDLE;
    }
    if (cubeShadowMapImage != VK_NULL_HANDLE) {
        vkDestroyImage(dev, cubeShadowMapImage, nullptr);
        cubeShadowMapImage = VK_NULL_HANDLE;
    }
    if (cubeShadowMapMemory != VK_NULL_HANDLE) {
        vkFreeMemory(dev, cubeShadowMapMemory, nullptr);
        cubeShadowMapMemory = VK_NULL_HANDLE;
    }

    // Dummy cube
    if (dummyCubeView != VK_NULL_HANDLE) {
        vkDestroyImageView(dev, dummyCubeView, nullptr);
        dummyCubeView = VK_NULL_HANDLE;
    }
    if (dummyCubeImage != VK_NULL_HANDLE) {
        vkDestroyImage(dev, dummyCubeImage, nullptr);
        dummyCubeImage = VK_NULL_HANDLE;
    }
    if (dummyCubeMemory != VK_NULL_HANDLE) {
        vkFreeMemory(dev, dummyCubeMemory, nullptr);
        dummyCubeMemory = VK_NULL_HANDLE;
    }

    // Directional shadow resources
    if (shadowPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(dev, shadowPipeline, nullptr);
        shadowPipeline = VK_NULL_HANDLE;
    }
    if (shadowPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(dev, shadowPipelineLayout, nullptr);
        shadowPipelineLayout = VK_NULL_HANDLE;
    }
    if (shadowFramebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(dev, shadowFramebuffer, nullptr);
        shadowFramebuffer = VK_NULL_HANDLE;
    }
    if (shadowRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(dev, shadowRenderPass, nullptr);
        shadowRenderPass = VK_NULL_HANDLE;
    }
    if (shadowMapSampler != VK_NULL_HANDLE) {
        vkDestroySampler(dev, shadowMapSampler, nullptr);
        shadowMapSampler = VK_NULL_HANDLE;
    }
    if (shadowMapPreviewView != VK_NULL_HANDLE) {
        vkDestroyImageView(dev, shadowMapPreviewView, nullptr);
        shadowMapPreviewView = VK_NULL_HANDLE;
    }
    if (shadowMapView != VK_NULL_HANDLE) {
        vkDestroyImageView(dev, shadowMapView, nullptr);
        shadowMapView = VK_NULL_HANDLE;
    }
    if (shadowMapImage != VK_NULL_HANDLE) {
        vkDestroyImage(dev, shadowMapImage, nullptr);
        shadowMapImage = VK_NULL_HANDLE;
    }
    if (shadowMapMemory != VK_NULL_HANDLE) {
        vkFreeMemory(dev, shadowMapMemory, nullptr);
        shadowMapMemory = VK_NULL_HANDLE;
    }

    std::cout << "ShadowPass cleaned up." << std::endl;
}
