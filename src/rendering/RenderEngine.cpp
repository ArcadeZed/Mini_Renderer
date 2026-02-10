#include "RenderEngine.h"
#include "../core/ShaderManager.h"
#include "../core/PipelineBuilder.h"
#include "../core/DescriptorManager.h"
#include "../mesh/MeshLoader.h"
#include "../mesh/PrimitiveMeshGenerator.h"
#include "ForwardPass.h"
#include "PhongLighting.h"
#include "BlinnPhongLighting.h"
#include "PBRLighting.h"
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

void RenderEngine::init(GLFWwindow* glfwWindow,
                        const std::string& meshPath,
                        const std::string& texturePath) {
    window = glfwWindow;
    meshFilePath = meshPath;
    textureFilePath = texturePath;
    initVulkan();
}

void RenderEngine::initVulkan() {
    // Load mesh data from file (CPU-side, before Vulkan init) - OPTIONAL
    if (!meshFilePath.empty()) {
        std::string resolvedPath = MeshLoader::resolvePath(meshFilePath);
        if (resolvedPath.empty()) {
            throw std::runtime_error("Could not find mesh file: " + meshFilePath);
        }
        meshFilePath = resolvedPath;

        SceneObject userObject;
        std::vector<Vertex> verts;
        std::vector<uint32_t> inds;
        std::vector<SubMesh> submeshes;
        std::vector<Material> materials;

        // Try loading with materials first (.obj files)
        if (!MeshLoader::loadFromFileWithMaterials(meshFilePath, verts, inds, submeshes, materials)) {
            throw std::runtime_error("Failed to load mesh file: " + meshFilePath);
        }

        // Extract filename from path for display name
        size_t lastSlash = meshFilePath.find_last_of("/\\");
        size_t lastDot = meshFilePath.find_last_of(".");
        std::string filename = meshFilePath.substr(lastSlash + 1, lastDot - lastSlash - 1);
        userObject.name = filename;

        // Set geometry with materials if available
        if (!submeshes.empty()) {
            userObject.mesh.setGeometryWithMaterials(std::move(verts), std::move(inds),
                                                       std::move(submeshes), std::move(materials));
            userObject.useMultiMaterial = true;
            std::cout << "Loaded mesh with " << userObject.mesh.getMaterials().size() << " materials" << std::endl;
        } else {
            userObject.mesh.setGeometry(std::move(verts), std::move(inds));
            userObject.useMultiMaterial = false;
            std::cout << "Loaded single-material mesh" << std::endl;
        }

        lastFileModTime = MeshLoader::getModTime(meshFilePath);
        scene.objects.push_back(std::move(userObject));
    } else {
        std::cout << "RenderEngine starting without mesh (use Drag & Drop to load)" << std::endl;
    }

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

    // Upload ALL scene meshes to GPU (if any loaded)
    for (auto& obj : scene.objects) {
        obj.mesh.upload(context.getDevice(), resource, command);
    }

    // Create uniform buffers, textures, descriptors
    createUniformBuffers();
    createTextureImage();  // Legacy default texture
    createTextureImageView();
    createTextureSampler();

    // Load material textures for each multi-material object
    for (auto& obj : scene.objects) {
        if (obj.useMultiMaterial) {
            loadMaterialTextures(obj);
        }
    }

    createDescriptorPool();
    createDescriptorSets();

    // Create material descriptor sets after pool creation (per-object)
    for (auto& obj : scene.objects) {
        if (obj.useMultiMaterial) {
            createMaterialDescriptorSets(obj);
        }
    }

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
    // Increased pool size to support multi-material meshes (up to 50 materials)
    const uint32_t MAX_MATERIAL_SETS = 50;
    const uint32_t TOTAL_SETS = MAX_FRAMES_IN_FLIGHT + MAX_MATERIAL_SETS;

    descriptorManager->createPool({
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, TOTAL_SETS},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, TOTAL_SETS}
    }, TOTAL_SETS);
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

    // Model matrix is now passed via push constants per-object, not via UBO

    // Update camera aspect ratio based on current swapchain extent
    float aspect = swapchain.getExtent().width / (float)swapchain.getExtent().height;
    scene.camera.setAspectRatio(aspect);

    ubo.view = scene.camera.getViewMatrix();
    ubo.proj = scene.camera.getProjectionMatrix();
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
    ubo.metalness = imguiParams.metalness;
    ubo.roughness = imguiParams.roughness;

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
    stbi_uc* pixels = nullptr;
    bool isDummyTexture = false;

    // Try loading texture from file if path is provided
    if (!textureFilePath.empty()) {
        pixels = stbi_load(textureFilePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        if (pixels) {
            std::cout << "Texture loaded: " << texWidth << "x" << texHeight
                      << " channels: " << texChannels << std::endl;
        }
    }

    // Create 1x1 white dummy texture if no file or load failed
    if (!pixels) {
        texWidth = texHeight = 1;
        texChannels = 4;
        pixels = new stbi_uc[4]{255, 255, 255, 255};  // White pixel
        isDummyTexture = true;
        std::cout << "Using default 1x1 white texture (no texture file provided)" << std::endl;
    }

    VkDeviceSize imageSize = texWidth * texHeight * 4;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    resource.createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(context.getDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(context.getDevice(), stagingBufferMemory);

    // Free pixel data (different for stbi vs dummy)
    if (isDummyTexture) {
        delete[] pixels;
    } else {
        stbi_image_free(pixels);
    }

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
    // Each object has its own material descriptor sets - no global list needed
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
    VkResult result = vkAcquireNextImageKHR(context.getDevice(), swapchain.getSwapchain(), UINT64_MAX,
                          imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Swapchain is out of date (e.g. window resized) - recreate it
        recreateSwapchain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swapchain image!");
    }

    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(context.getDevice(), 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];

    updateUniformBuffer(currentFrame);

    // ImGui frame
    imguiOverlay->beginFrame();
    imguiOverlay->buildUI(imguiParams, scene, selectedObjectIndex, gizmoState, this);

    // Check if lighting model changed and rebuild pipeline if needed
    if (imguiParams.lightingModelIndex != previousLightingModelIndex) {
        // Wait for device to be idle before rebuilding pipeline
        vkDeviceWaitIdle(context.getDevice());

        static PhongLighting phong;
        static BlinnPhongLighting blinnPhong;
        static PBRLighting pbr;

        ILightingModel* models[] = { &phong, &blinnPhong, &pbr };
        int index = std::clamp(imguiParams.lightingModelIndex, 0, 2);
        forwardPass->setLightingModel(models[index]);

        previousLightingModelIndex = imguiParams.lightingModelIndex;
    }

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

    result = vkQueuePresentKHR(context.getPresentQueue(), &presentInfo);

    // Check if swapchain needs recreation (window resized or suboptimal)
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;  // Reset flag
        recreateSwapchain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swapchain image!");
    }

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
        std::vector<uint32_t> inds;
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

    // Scene mesh GPU buffers + material textures (per-object cleanup)
    scene.cleanup(context.getDevice());

    // Legacy texture cleanup (fallback for objects without materials)
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

// ============================================================================
// SWAPCHAIN RECREATION (for window resize)
// ============================================================================

void RenderEngine::recreateSwapchain() {
    // Handle minimized window (width/height = 0)
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        // Wait until window is restored
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    // Wait for device to finish current operations
    vkDeviceWaitIdle(context.getDevice());

    // Cleanup old swapchain-dependent resources
    for (auto framebuffer : swapchainFramebuffers) {
        vkDestroyFramebuffer(context.getDevice(), framebuffer, nullptr);
    }
    swapchainFramebuffers.clear();

    vkDestroyImageView(context.getDevice(), depthImageView, nullptr);
    vkDestroyImage(context.getDevice(), depthImage, nullptr);
    vkFreeMemory(context.getDevice(), depthImageMemory, nullptr);

    swapchain.cleanup();

    // Recreate swapchain and dependent resources
    swapchain.init(&context, window);
    createDepthResources();
    createFramebuffers();

    // Update pipelines with new extent (if they use fixed viewport/scissor)
    forwardPass->updateExtent(swapchain.getExtent());
    debugRenderer->updateExtent(swapchain.getExtent());

    std::cout << "Swapchain recreated (new resolution: "
              << swapchain.getExtent().width << "x" << swapchain.getExtent().height
              << ")" << std::endl;
}

// ============================================================================
// DYNAMIC LOADING (Runtime mesh/texture loading)
// ============================================================================

void RenderEngine::loadMesh(const std::string& filepath) {
    std::cout << "Loading new mesh: " << filepath << std::endl;

    // Wait for GPU to finish all work
    vkDeviceWaitIdle(context.getDevice());

    // Load new mesh data from file (with material support)
    std::vector<Vertex> verts;
    std::vector<uint32_t> inds;
    std::vector<SubMesh> submeshes;
    std::vector<Material> materials;

    std::string resolvedPath = MeshLoader::resolvePath(filepath);
    if (resolvedPath.empty()) {
        std::cerr << "Could not find mesh file: " << filepath << std::endl;
        return;
    }

    if (!MeshLoader::loadFromFileWithMaterials(resolvedPath, verts, inds, submeshes, materials)) {
        std::cerr << "Failed to load mesh file: " << resolvedPath << std::endl;
        return;
    }

    // Update mesh file path for hot-reload (tracks last loaded mesh)
    meshFilePath = resolvedPath;
    lastFileModTime = MeshLoader::getModTime(resolvedPath);

    // Create NEW scene object (add to scene instead of replacing)
    SceneObject newObject;
    newObject.transform.position = glm::vec3(0.0f, 0.0f, 0.0f);  // Default position

    // Extract filename from path for display name
    size_t lastSlash = resolvedPath.find_last_of("/\\");
    size_t lastDot = resolvedPath.find_last_of(".");
    std::string filename = resolvedPath.substr(lastSlash + 1, lastDot - lastSlash - 1);
    newObject.name = filename;

    // Set geometry with materials if available
    if (!submeshes.empty()) {
        newObject.mesh.setGeometryWithMaterials(std::move(verts), std::move(inds),
                                                 std::move(submeshes), std::move(materials));
        newObject.useMultiMaterial = true;
        std::cout << "Loaded mesh with " << newObject.mesh.getMaterials().size() << " materials" << std::endl;
    } else {
        newObject.mesh.setGeometry(std::move(verts), std::move(inds));
        newObject.useMultiMaterial = false;
        std::cout << "Loaded single-material mesh" << std::endl;
    }

    // Upload new mesh to GPU
    newObject.mesh.upload(context.getDevice(), resource, command);

    // Load material textures and create descriptor sets (per-object)
    if (newObject.useMultiMaterial) {
        loadMaterialTextures(newObject);
        createMaterialDescriptorSets(newObject);
    }

    // Add to scene
    scene.objects.push_back(std::move(newObject));
    std::cout << "Mesh added to scene (total objects: " << scene.objects.size() << ")" << std::endl;
    std::cout << "Mesh loaded successfully: " << resolvedPath << std::endl;
}

void RenderEngine::deleteObject(size_t index) {
    if (index >= scene.objects.size()) {
        std::cerr << "Invalid object index: " << index << std::endl;
        return;
    }

    // Wait for GPU to finish using this object
    vkDeviceWaitIdle(context.getDevice());

    // Cleanup GPU resources for this object
    scene.objects[index].mesh.cleanup(context.getDevice());
    scene.objects[index].cleanupMaterialResources(context.getDevice());

    // Remove from scene
    scene.objects.erase(scene.objects.begin() + index);

    // Update selection if needed
    if (selectedObjectIndex == static_cast<int>(index)) {
        selectedObjectIndex = -1;  // Deselect if deleted object was selected
    } else if (selectedObjectIndex > static_cast<int>(index)) {
        selectedObjectIndex--;  // Adjust index if object before selection was deleted
    }

    std::cout << "Object " << index << " deleted (remaining: " << scene.objects.size() << ")" << std::endl;
}

void RenderEngine::addPrimitive(PrimitiveType type,
                                 const std::string& name,
                                 const glm::vec3& position) {
    // 1. Generate geometry
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    switch (type) {
        case PrimitiveType::Sphere:
            PrimitiveMeshGenerator::generateSphere(vertices, indices, 1.0f, 32, 64);
            break;
        case PrimitiveType::Cube:
            PrimitiveMeshGenerator::generateCube(vertices, indices, 1.0f);
            break;
        case PrimitiveType::Plane:
            PrimitiveMeshGenerator::generatePlane(vertices, indices, 10.0f, 10.0f, 1);
            break;
    }

    // Save sizes before move
    size_t vertexCount = vertices.size();
    size_t indexCount = indices.size();

    // 2. Create SceneObject
    SceneObject newObject;
    newObject.name = name;
    newObject.transform.position = position;
    newObject.mesh.setGeometry(std::move(vertices), std::move(indices));
    newObject.useMultiMaterial = false;

    // 3. Upload mesh to GPU
    newObject.mesh.upload(context.getDevice(), resource, command);

    // 4. Load default material (reuse existing fallback texture)
    loadMaterialTextures(newObject);  // Existing method handles fallback
    createMaterialDescriptorSets(newObject);  // Existing method

    // 5. Add to scene
    scene.objects.push_back(std::move(newObject));

    std::cout << "Primitive added: " << name
              << " (vertices: " << vertexCount
              << ", indices: " << indexCount
              << ", total objects: " << scene.objects.size() << ")" << std::endl;
}

void RenderEngine::loadTexture(const std::string& filepath) {
    std::cout << "Loading new texture: " << filepath << std::endl;

    // Wait for GPU to finish all work (this is sufficient!)
    vkDeviceWaitIdle(context.getDevice());

    // Cleanup old texture resources
    vkDestroyImageView(context.getDevice(), textureImageView, nullptr);
    vkDestroyImage(context.getDevice(), textureImage, nullptr);
    vkFreeMemory(context.getDevice(), textureImageMemory, nullptr);

    // Update texture file path
    textureFilePath = filepath;

    // Load new texture (same code as createTextureImage)
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(textureFilePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        std::cerr << "Failed to load texture image: " << textureFilePath << std::endl;
        // Restore to default/previous texture on failure
        return;
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

    // Recreate image view with new image
    textureImageView = resource.createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, mipLevels);

    // Update descriptor sets to use new texture
    // IMPORTANT: Loop over MAX_FRAMES_IN_FLIGHT, not swapchain images!
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureImageView;
        imageInfo.sampler = textureSampler;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSets[i];
        descriptorWrite.dstBinding = 1;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(context.getDevice(), 1, &descriptorWrite, 0, nullptr);
    }

    std::cout << "Texture loaded successfully: " << filepath << std::endl;
}
// This file will be included at the end of RenderEngine.cpp
// Contains multi-material texture loading functions

// ============================================================================
// MULTI-MATERIAL TEXTURE LOADING
// ============================================================================

void RenderEngine::loadMaterialTextures(SceneObject& object) {
    const auto& materials = object.mesh.getMaterials();

    if (materials.empty()) {
        std::cerr << "No materials found in mesh!" << std::endl;
        return;
    }

    object.materialResources.clear();
    object.materialResources.reserve(materials.size());

    std::cout << "Loading " << materials.size() << " material textures for object..." << std::endl;

    for (size_t i = 0; i < materials.size(); i++) {
        const Material& mat = materials[i];
        MaterialResources matRes;

        std::cout << "  Material [" << i << "]: " << mat.name;

        // Check if material has a texture
        if (mat.diffuseTexturePath.empty()) {
            std::cout << " - No texture, using default" << std::endl;
            // Use default white texture (or fallback to legacy textureImage)
            matRes.textureImage = textureImage;  // Fallback
            matRes.textureImageView = textureImageView;
            matRes.textureImageMemory = VK_NULL_HANDLE;  // Don't own it
            matRes.mipLevels = mipLevels;
            object.materialResources.push_back(matRes);
            continue;
        }

        std::cout << " - Texture: " << mat.diffuseTexturePath << std::endl;

        // Load texture from file
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(mat.diffuseTexturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

        if (!pixels) {
            std::cerr << "    Failed to load texture! Using default." << std::endl;
            // Fallback to default texture
            matRes.textureImage = textureImage;
            matRes.textureImageView = textureImageView;
            matRes.textureImageMemory = VK_NULL_HANDLE;
            matRes.mipLevels = mipLevels;
            object.materialResources.push_back(matRes);
            continue;
        }

        VkDeviceSize imageSize = texWidth * texHeight * 4;
        std::cout << "    Loaded: " << texWidth << "x" << texHeight << " channels: " << texChannels << std::endl;

        // Create staging buffer
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

        // Calculate mip levels
        matRes.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

        // Create image
        resource.createImage(texWidth, texHeight, matRes.mipLevels, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    matRes.textureImage, matRes.textureImageMemory);

        // Transition layout and copy
        resource.transitionImageLayout(matRes.textureImage, VK_FORMAT_R8G8B8A8_SRGB,
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        resource.copyBufferToImage(stagingBuffer, matRes.textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

        // Generate mipmaps
        generateMipmaps(matRes.textureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, matRes.mipLevels);

        // Cleanup staging buffer
        vkDestroyBuffer(context.getDevice(), stagingBuffer, nullptr);
        vkFreeMemory(context.getDevice(), stagingBufferMemory, nullptr);

        // Create image view
        matRes.textureImageView = resource.createImageView(matRes.textureImage, VK_FORMAT_R8G8B8A8_SRGB, matRes.mipLevels);

        std::cout << "    Created VkImage + VkImageView with " << matRes.mipLevels << " mip levels" << std::endl;

        object.materialResources.push_back(matRes);
    }

    std::cout << "All material textures loaded successfully for this object!" << std::endl;
}

void RenderEngine::createMaterialDescriptorSets(SceneObject& object) {
    if (object.materialResources.empty()) {
        std::cerr << "No material resources loaded for this object!" << std::endl;
        return;
    }

    std::cout << "Creating " << object.materialResources.size() << " descriptor sets (one per material)..." << std::endl;

    // Allocate descriptor sets using DescriptorManager
    std::vector<VkDescriptorSet> tempSets = descriptorManager->allocateSets(
        descriptorSetLayout, static_cast<uint32_t>(object.materialResources.size()));

    // Update each material's descriptor set
    for (size_t i = 0; i < object.materialResources.size(); i++) {
        object.materialResources[i].descriptorSet = tempSets[i];

        // Binding 0: UBO (same for all materials - use frame 0's UBO)
        descriptorManager->writeBuffer(object.materialResources[i].descriptorSet, 0,
                                        uniformBuffers[0], sizeof(UniformBufferObject));

        // Binding 1: Texture sampler
        descriptorManager->writeImage(object.materialResources[i].descriptorSet, 1,
                                       object.materialResources[i].textureImageView, textureSampler);

        std::cout << "  Descriptor set [" << i << "] created for material" << std::endl;
    }

    std::cout << "All material descriptor sets created for this object!" << std::endl;
}

