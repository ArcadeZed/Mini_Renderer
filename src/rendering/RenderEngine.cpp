#include "RenderEngine.h"
#include "../core/ShaderManager.h"
#include "../core/PipelineBuilder.h"
#include "../core/DescriptorManager.h"
#include "../mesh/MeshLoader.h"
#include "ForwardPass.h"
#include "../debug/DebugRenderer.h"
#include "../debug/ImGuiOverlay.h"
#include <stdexcept>
#include <iostream>
#include <vector>
#include <array>
#include <cstring>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "../../external/stb_image.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// ============================================================================
// RENDER ENGINE IMPLEMENTATION
// ============================================================================

RenderEngine::RenderEngine() {}
RenderEngine::~RenderEngine() {}

void RenderEngine::init(GLFWwindow* glfwWindow) {
    window = glfwWindow;
    initVulkan();
}

void RenderEngine::initVulkan() {
    // Load mesh data from file (CPU-side, before Vulkan init)
    std::string resolvedPath = MeshLoader::resolvePath("../models/triangle.txt");
    if (resolvedPath.empty()) {
        throw std::runtime_error("Could not find mesh file: ../models/triangle.txt");
    }
    meshFilePath = resolvedPath;

    SceneObject userObject;
    std::vector<Vertex> verts;
    std::vector<uint16_t> inds;
    if (!MeshLoader::loadFromFile(meshFilePath, verts, inds)) {
        throw std::runtime_error("Failed to load mesh file: " + meshFilePath);
    }
    userObject.mesh.setGeometry(std::move(verts), std::move(inds));
    lastFileModTime = MeshLoader::getModTime(meshFilePath);
    scene.objects.push_back(std::move(userObject));

    // Initialize Vulkan Core (Instance, Device, Queues, Surface)
    context.init(window, enableValidationLayers);

    // Initialize managers
    shaderManager = std::make_unique<ShaderManager>(context.getDevice());
    descriptorManager = std::make_unique<DescriptorManager>(context.getDevice());

    // Initialize Swapchain, Command, Resource
    swapchain.init(&context, window);
    command.init(&context);
    resource.init(&context, &command);

    // Create shared Vulkan resources
    createRenderPass();
    createDepthResources();
    createDescriptorSetLayout();
    createFramebuffers();

    // Upload scene mesh to GPU
    scene.objects[0].mesh.upload(context.getDevice(), resource, command);

    // Create uniform buffers, textures, descriptors
    createUniformBuffers();
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
    createDescriptorPool();
    createDescriptorSets();

    // Initialize render sub-systems (AFTER render pass + descriptor layout exist)
    forwardPass = std::make_unique<ForwardPass>();
    forwardPass->init(context.getDevice(), *shaderManager,
                      swapchain.getExtent(), descriptorSetLayout, renderPass);

    debugRenderer = std::make_unique<DebugRenderer>();
    debugRenderer->init(context.getDevice(), *shaderManager,
                        swapchain.getExtent(), descriptorSetLayout, renderPass,
                        resource, command);

    imguiOverlay = std::make_unique<ImGuiOverlay>();
    imguiOverlay->init(window, context, renderPass,
                       static_cast<uint32_t>(swapchain.getImages().size()));

    // Allocate command buffers (per-frame recording, not pre-recorded)
    command.allocateCommandBuffers(commandBuffers,
                                   static_cast<uint32_t>(swapchainFramebuffers.size()));

    createSyncObjects();
}

// ============================================================================
// RENDER PASS
// ============================================================================

void RenderEngine::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchain.getImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(context.getDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
    std::cout << "Render pass created." << std::endl;
}

// ============================================================================
// DESCRIPTORS & UNIFORMS
// ============================================================================

void RenderEngine::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 1;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    descriptorSetLayout = descriptorManager->createLayout({uboBinding, samplerBinding});
}

void RenderEngine::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        resource.createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     uniformBuffers[i], uniformBuffersMemory[i]);
    }
    std::cout << "Uniform buffers created." << std::endl;
}

void RenderEngine::createDescriptorPool() {
    descriptorManager->createPool({
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)}
    }, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT));
}

void RenderEngine::createDescriptorSets() {
    descriptorSets = descriptorManager->allocateSets(descriptorSetLayout,
                                                      static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT));

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        descriptorManager->writeBuffer(descriptorSets[i], 0,
                                        uniformBuffers[i], sizeof(UniformBufferObject));
        descriptorManager->writeImage(descriptorSets[i], 1,
                                       textureImageView, textureSampler);
    }
    std::cout << "Descriptor sets created." << std::endl;
}

void RenderEngine::updateUniformBuffer(uint32_t currentImage) {
    UniformBufferObject ubo{};

    ubo.model = scene.objects[0].transform.getModelMatrix();

    float aspect = swapchain.getExtent().width / (float)swapchain.getExtent().height;
    ubo.view = scene.camera.getViewMatrix();
    ubo.proj = scene.camera.getProjectionMatrix(aspect);
    ubo.viewPos = scene.camera.getPosition();

    // Read from ImGui interactive params
    ubo.lightPos = imguiParams.lightPos;
    ubo.lightColor = imguiParams.lightColor;
    ubo.ambientStrength = imguiParams.ambientStrength;
    ubo.shininess = imguiParams.shininess;
    ubo.lightIntensity = imguiParams.lightIntensity;
    ubo.attenuationConstant = imguiParams.attenuationConstant;
    ubo.attenuationLinear = imguiParams.attenuationLinear;
    ubo.attenuationQuadratic = imguiParams.attenuationQuadratic;

    void* data;
    vkMapMemory(context.getDevice(), uniformBuffersMemory[currentImage], 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(context.getDevice(), uniformBuffersMemory[currentImage]);
}

// ============================================================================
// FRAMEBUFFERS & DEPTH
// ============================================================================

void RenderEngine::createFramebuffers() {
    swapchainFramebuffers.resize(swapchain.getImageViews().size());

    for (size_t i = 0; i < swapchain.getImageViews().size(); i++) {
        std::array<VkImageView, 2> attachments = {
                swapchain.getImageViews()[i],
                depthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapchain.getExtent().width;
        framebufferInfo.height = swapchain.getExtent().height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(context.getDevice(), &framebufferInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
    std::cout << "Framebuffers created." << std::endl;
}

void RenderEngine::createDepthResources() {
    VkFormat depthFormat = findDepthFormat();

    resource.createImage(swapchain.getExtent().width, swapchain.getExtent().height, 1, depthFormat, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            depthImage, depthImageMemory);

    depthImageView = resource.createImageView(depthImage, depthFormat);
    std::cout << "Depth resources created." << std::endl;
}

VkFormat RenderEngine::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(context.getPhysicalDevice(), format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format!");
}

VkFormat RenderEngine::findDepthFormat() {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

bool RenderEngine::hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

// ============================================================================
// TEXTURES
// ============================================================================

void RenderEngine::createTextureImage() {
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("textures/test_texture.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    std::cout << "Texture loaded: " << texWidth << "x" << texHeight
              << " channels: " << texChannels << std::endl;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    resource.createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(context.getDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(context.getDevice(), stagingBufferMemory);

    stbi_image_free(pixels);

    mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

    resource.createImage(texWidth, texHeight, mipLevels, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                textureImage, textureImageMemory);

    resource.transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    resource.copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

    generateMipmaps(textureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, mipLevels);

    vkDestroyBuffer(context.getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(context.getDevice(), stagingBufferMemory, nullptr);

    std::cout << "Texture image created." << std::endl;
}

void RenderEngine::createTextureImageView() {
    textureImageView = resource.createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, mipLevels);
}

void RenderEngine::createTextureSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(context.getPhysicalDevice(), &properties);
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(mipLevels);

    if (vkCreateSampler(context.getDevice(), &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
}

void RenderEngine::generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(context.getPhysicalDevice(), imageFormat, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    VkCommandBuffer commandBuffer = command.beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr, 0, nullptr, 1, &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit, VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr, 0, nullptr, 1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr, 0, nullptr, 1, &barrier);

    command.endSingleTimeCommands(commandBuffer);
}

// ============================================================================
// SYNCHRONIZATION
// ============================================================================

void RenderEngine::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(swapchain.getImages().size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(context.getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(context.getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(context.getDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects!");
        }
    }
    std::cout << "Synchronization objects created." << std::endl;
}

// ============================================================================
// COMMAND BUFFER RECORDING (PER-FRAME)
// ============================================================================

void RenderEngine::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapchainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchain.getExtent();

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // 1. Scene objects (Phong + texture)
    forwardPass->record(cmd, descriptorSets[currentFrame % MAX_FRAMES_IN_FLIGHT], scene);

    // 2. Debug visualization (grid + axes) - conditional on ImGui toggles
    debugRenderer->record(cmd, descriptorSets[currentFrame % MAX_FRAMES_IN_FLIGHT],
                          imguiParams.showGrid, imguiParams.showDebugAxes);

    // 3. ImGui overlay (renders on top)
    imguiOverlay->record(cmd);

    vkCmdEndRenderPass(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer!");
    }
}

// ============================================================================
// FRAME LOOP
// ============================================================================

void RenderEngine::drawFrame() {
    vkWaitForFences(context.getDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(context.getDevice(), swapchain.getSwapchain(), UINT64_MAX,
                          imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(context.getDevice(), 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];

    updateUniformBuffer(currentFrame);

    // ImGui frame
    imguiOverlay->beginFrame();
    imguiOverlay->buildUI(imguiParams);

    // Per-frame command buffer recording
    recordCommandBuffer(commandBuffers[imageIndex], imageIndex);

    // Submit
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkResetFences(context.getDevice(), 1, &inFlightFences[currentFrame]);

    if (vkQueueSubmit(context.getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {swapchain.getSwapchain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(context.getPresentQueue(), &presentInfo);
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// ============================================================================
// HOT RELOAD
// ============================================================================

void RenderEngine::checkAndReloadMesh() {
    std::time_t modTime = MeshLoader::getModTime(meshFilePath);
    if (modTime == 0 || modTime <= lastFileModTime) return;

    std::cout << "Mesh file changed! Reloading..." << std::endl;

    try {
        std::vector<Vertex> verts;
        std::vector<uint16_t> inds;
        if (!MeshLoader::loadFromFile(meshFilePath, verts, inds)) {
            std::cerr << "Failed to reload mesh file." << std::endl;
            return;
        }

        scene.objects[0].mesh.setGeometry(std::move(verts), std::move(inds));
        lastFileModTime = modTime;

        recreateBuffers();
        std::cout << "Mesh reloaded successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to reload mesh: " << e.what() << std::endl;
    }
}

void RenderEngine::recreateBuffers() {
    vkDeviceWaitIdle(context.getDevice());

    scene.objects[0].mesh.cleanup(context.getDevice());
    scene.objects[0].mesh.upload(context.getDevice(), resource, command);
    // No command buffer re-recording needed - per-frame recording handles it
}

// ============================================================================
// CLEANUP
// ============================================================================

void RenderEngine::cleanup() {
    vkDeviceWaitIdle(context.getDevice());

    // Cleanup render sub-systems (own pipelines/meshes)
    imguiOverlay->cleanup(context.getDevice());
    debugRenderer->cleanup(context.getDevice());
    forwardPass->cleanup(context.getDevice());

    // Sync objects
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(context.getDevice(), renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(context.getDevice(), imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(context.getDevice(), inFlightFences[i], nullptr);
    }

    // Command pool (frees all command buffers)
    command.cleanup();

    // Scene mesh GPU buffers
    scene.cleanup(context.getDevice());

    // Texture cleanup
    vkDestroySampler(context.getDevice(), textureSampler, nullptr);
    vkDestroyImageView(context.getDevice(), textureImageView, nullptr);
    vkDestroyImage(context.getDevice(), textureImage, nullptr);
    vkFreeMemory(context.getDevice(), textureImageMemory, nullptr);

    // Uniform buffers
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(context.getDevice(), uniformBuffers[i], nullptr);
        vkFreeMemory(context.getDevice(), uniformBuffersMemory[i], nullptr);
    }

    // Framebuffers
    for (auto framebuffer : swapchainFramebuffers) {
        vkDestroyFramebuffer(context.getDevice(), framebuffer, nullptr);
    }

    // Depth resources
    vkDestroyImageView(context.getDevice(), depthImageView, nullptr);
    vkDestroyImage(context.getDevice(), depthImage, nullptr);
    vkFreeMemory(context.getDevice(), depthImageMemory, nullptr);

    // Descriptor manager (pool + layouts)
    descriptorManager.reset();

    // Render pass
    vkDestroyRenderPass(context.getDevice(), renderPass, nullptr);

    // Shader manager
    shaderManager.reset();

    // Swapchain - BEFORE VulkanContext
    swapchain.cleanup();

    // VulkanContext - MUST BE LAST
    context.cleanup();

    std::cout << "RenderEngine cleaned up." << std::endl;
}
