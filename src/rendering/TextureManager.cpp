#include "TextureManager.h"
#include "core/VulkanContext.h"
#include "core/VulkanCommand.h"
#include "core/VulkanResource.h"

// Note: STB_IMAGE_IMPLEMENTATION is already defined in RenderEngine.cpp
#include "stb_image.h"

#include <iostream>
#include <cmath>
#include <algorithm>

TextureManager::TextureManager(VulkanContext* context, VulkanCommand* command, VulkanResource* resource)
    : m_context(context), m_command(command), m_resource(resource) {
    // Create default 1x1 white texture at index 0
    createDefaultTexture();
}

TextureManager::~TextureManager() {
    cleanup();
}

void TextureManager::createDefaultTexture() {
    unsigned char whitePixel[4] = {255, 255, 255, 255};
    GPUTexture defaultTex = createTextureFromData(whitePixel, 1, 1, 4, false);
    m_textures.push_back(defaultTex);
    std::cout << "[TextureManager] Default white texture created at index 0" << std::endl;
}

int TextureManager::loadTexture(const std::string& filepath, bool generateMipmaps) {
    // Check cache
    auto it = m_textureCache.find(filepath);
    if (it != m_textureCache.end()) {
        std::cout << "[TextureManager] Texture cached: " << filepath << " (index " << it->second << ")" << std::endl;
        return it->second;
    }

    // Load image with stb_image
    int width, height, channels;
    stbi_set_flip_vertically_on_load(false);  // glTF uses top-left origin
    unsigned char* pixels = stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels) {
        std::cerr << "[TextureManager] Failed to load texture: " << filepath << std::endl;
        return 0;  // Return default white texture
    }

    std::cout << "[TextureManager] Loading: " << filepath << " (" << width << "x" << height << ", " << channels << " channels)";

    // Downsample if exceeds max texture size
    unsigned char* finalPixels = pixels;
    int finalWidth = width;
    int finalHeight = height;

    if (m_maxTextureSize > 0 && (width > (int)m_maxTextureSize || height > (int)m_maxTextureSize)) {
        // Calculate target size (power of 2 reduction)
        int targetWidth = width;
        int targetHeight = height;
        while (targetWidth > (int)m_maxTextureSize || targetHeight > (int)m_maxTextureSize) {
            targetWidth /= 2;
            targetHeight /= 2;
        }

        // Simple box filter downsample
        finalPixels = new unsigned char[targetWidth * targetHeight * 4];
        float scaleX = (float)width / targetWidth;
        float scaleY = (float)height / targetHeight;

        for (int y = 0; y < targetHeight; y++) {
            for (int x = 0; x < targetWidth; x++) {
                int srcX = (int)(x * scaleX);
                int srcY = (int)(y * scaleY);
                int srcIdx = (srcY * width + srcX) * 4;
                int dstIdx = (y * targetWidth + x) * 4;

                finalPixels[dstIdx + 0] = pixels[srcIdx + 0];
                finalPixels[dstIdx + 1] = pixels[srcIdx + 1];
                finalPixels[dstIdx + 2] = pixels[srcIdx + 2];
                finalPixels[dstIdx + 3] = pixels[srcIdx + 3];
            }
        }

        finalWidth = targetWidth;
        finalHeight = targetHeight;
        std::cout << " → downsampled to " << finalWidth << "x" << finalHeight;
    }
    std::cout << std::endl;

    // Create GPU texture
    GPUTexture gpuTex = createTextureFromData(finalPixels, finalWidth, finalHeight, 4, generateMipmaps);

    // Free memory
    if (finalPixels != pixels) {
        delete[] finalPixels;
    }
    stbi_image_free(pixels);

    // Store and cache
    int index = static_cast<int>(m_textures.size());
    m_textures.push_back(gpuTex);
    m_textureCache[filepath] = index;

    return index;
}

std::vector<int> TextureManager::loadTextures(const std::vector<std::string>& filepaths, bool generateMipmaps) {
    std::vector<int> indices;
    indices.reserve(filepaths.size());

    std::cout << "[TextureManager] Batch loading " << filepaths.size() << " textures..." << std::endl;

    for (const auto& filepath : filepaths) {
        indices.push_back(loadTexture(filepath, generateMipmaps));
    }

    std::cout << "[TextureManager] Batch load complete. Total textures: " << m_textures.size() << std::endl;

    return indices;
}

const GPUTexture& TextureManager::getTexture(int index) const {
    if (index < 0 || index >= static_cast<int>(m_textures.size())) {
        return m_textures[0];  // Return default texture
    }
    return m_textures[index];
}

GPUTexture TextureManager::createTextureFromData(
    const unsigned char* pixels,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    bool generateMipmaps
) {
    GPUTexture texture;
    texture.width = width;
    texture.height = height;
    texture.mipLevels = generateMipmaps ? calculateMipLevels(width, height) : 1;

    VkDevice device = m_context->getDevice();
    VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 4;

    // Create staging buffer using VulkanResource
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    m_resource->createBuffer(
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory
    );

    // Copy pixel data to staging buffer
    void* data;
    vkMapMemory(device, stagingMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingMemory);

    // Create image using VulkanResource
    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if (generateMipmaps) {
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;  // For mipmap generation
    }

    m_resource->createImage(
        width, height, texture.mipLevels,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        usage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        texture.image, texture.memory
    );

    // Transition image layout and copy buffer to image
    m_resource->transitionImageLayout(
        texture.image,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    m_resource->copyBufferToImage(
        stagingBuffer,
        texture.image,
        width, height
    );

    // Generate mipmaps or transition to SHADER_READ_ONLY
    if (generateMipmaps) {
        generateMipmapsForTexture(texture.image, VK_FORMAT_R8G8B8A8_SRGB, width, height, texture.mipLevels);
    } else {
        // Transition to SHADER_READ_ONLY_OPTIMAL
        m_resource->transitionImageLayout(
            texture.image,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
    }

    // Cleanup staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    // Create image view using VulkanResource
    texture.imageView = m_resource->createImageView(
        texture.image,
        VK_FORMAT_R8G8B8A8_SRGB,
        texture.mipLevels
    );

    // Create sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(texture.mipLevels);

    if (vkCreateSampler(device, &samplerInfo, nullptr, &texture.sampler) != VK_SUCCESS) {
        std::cerr << "[TextureManager] Failed to create sampler!" << std::endl;
    }

    return texture;
}

void TextureManager::generateMipmapsForTexture(VkImage image, VkFormat format, uint32_t width, uint32_t height, uint32_t mipLevels) {
    VkCommandBuffer cmdBuffer = m_command->beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = static_cast<int32_t>(width);
    int32_t mipHeight = static_cast<int32_t>(height);

    for (uint32_t i = 1; i < mipLevels; i++) {
        // Transition previous mip level to TRANSFER_SRC_OPTIMAL
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(
            cmdBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        // Blit from previous mip level to current
        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(
            cmdBuffer,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR
        );

        // Transition previous mip level to SHADER_READ_ONLY_OPTIMAL
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            cmdBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    // Transition last mip level to SHADER_READ_ONLY_OPTIMAL
    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        cmdBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    m_command->endSingleTimeCommands(cmdBuffer);
}

uint32_t TextureManager::calculateMipLevels(uint32_t width, uint32_t height) {
    return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
}

void TextureManager::cleanup() {
    VkDevice device = m_context->getDevice();

    for (auto& texture : m_textures) {
        if (texture.sampler != VK_NULL_HANDLE) {
            vkDestroySampler(device, texture.sampler, nullptr);
        }
        if (texture.imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, texture.imageView, nullptr);
        }
        if (texture.image != VK_NULL_HANDLE) {
            vkDestroyImage(device, texture.image, nullptr);
        }
        if (texture.memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, texture.memory, nullptr);
        }
    }

    m_textures.clear();
    m_textureCache.clear();

    std::cout << "[TextureManager] Cleaned up all textures" << std::endl;
}
