#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <unordered_map>

class VulkanContext;
class VulkanCommand;
class VulkanResource;

struct GPUTexture {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipLevels = 1;

    bool isValid() const { return image != VK_NULL_HANDLE; }
};

class TextureManager {
public:
    TextureManager(VulkanContext* context, VulkanCommand* command, VulkanResource* resource);
    ~TextureManager();

    // Load single texture from file
    int loadTexture(const std::string& filepath, bool generateMipmaps = true);

    // Batch load multiple textures (more efficient)
    std::vector<int> loadTextures(const std::vector<std::string>& filepaths, bool generateMipmaps = true);

    // Get texture by index (returned from loadTexture)
    const GPUTexture& getTexture(int index) const;

    // Get texture count
    size_t getTextureCount() const { return m_textures.size(); }

    // Get default white texture (1x1 white pixel)
    int getDefaultWhiteTexture() const { return 0; }

    // Set max texture size (0 = no limit, e.g. 2048 to limit to 2048x2048)
    void setMaxTextureSize(uint32_t maxSize) { m_maxTextureSize = maxSize; }

    // Cleanup
    void cleanup();

private:
    VulkanContext* m_context;
    VulkanCommand* m_command;
    VulkanResource* m_resource;

    std::vector<GPUTexture> m_textures;
    std::unordered_map<std::string, int> m_textureCache;  // filepath -> index
    uint32_t m_maxTextureSize = 0;  // 0 = no limit, else max width/height

    // Create texture from raw pixel data
    GPUTexture createTextureFromData(
        const unsigned char* pixels,
        uint32_t width,
        uint32_t height,
        uint32_t channels,
        bool generateMipmaps
    );

    // Generate mipmaps for a texture
    void generateMipmapsForTexture(VkImage image, VkFormat format, uint32_t width, uint32_t height, uint32_t mipLevels);

    // Calculate mip levels
    static uint32_t calculateMipLevels(uint32_t width, uint32_t height);

    // Create default 1x1 white texture
    void createDefaultTexture();
};
