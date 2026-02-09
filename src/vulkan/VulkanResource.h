#pragma once

#include <vulkan/vulkan.h>

// Forward declarations
class VulkanContext;
class VulkanCommand;

/**
 * @brief Manages Vulkan resource creation: Buffers and Images
 *
 * Encapsulates low-level Vulkan boilerplate for:
 * - Buffer creation with memory allocation
 * - Image creation with memory allocation
 * - Image view creation
 * - Layout transitions
 * - Buffer-to-image copies
 *
 * This is the foundation layer used by Mesh, Texture, and other systems.
 */
class VulkanResource {
public:
    VulkanResource() = default;
    ~VulkanResource() = default;

    // No copy
    VulkanResource(const VulkanResource&) = delete;
    VulkanResource& operator=(const VulkanResource&) = delete;

    /**
     * @brief Initialize resource manager
     * @param ctx Pointer to VulkanContext
     * @param cmd Pointer to VulkanCommand (for transfer operations)
     */
    void init(VulkanContext* ctx, VulkanCommand* cmd);

    // No cleanup needed - this module doesn't own resources, just creates them

    // ========================================================================
    // BUFFER OPERATIONS
    // ========================================================================

    /**
     * @brief Create a Vulkan buffer with memory allocation
     * @param size Buffer size in bytes
     * @param usage Buffer usage flags (e.g., VERTEX_BUFFER_BIT)
     * @param properties Memory properties (e.g., DEVICE_LOCAL_BIT)
     * @param buffer Output: created buffer handle
     * @param bufferMemory Output: allocated memory handle
     */
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags properties,
                     VkBuffer& buffer, VkDeviceMemory& bufferMemory);

    // ========================================================================
    // IMAGE OPERATIONS
    // ========================================================================

    /**
     * @brief Create a Vulkan image with memory allocation
     * @param width Image width
     * @param height Image height
     * @param mipLevels Number of mipmap levels
     * @param format Image format (e.g., VK_FORMAT_R8G8B8A8_SRGB)
     * @param tiling Image tiling (LINEAR or OPTIMAL)
     * @param usage Image usage flags (e.g., SAMPLED_BIT)
     * @param properties Memory properties (e.g., DEVICE_LOCAL_BIT)
     * @param image Output: created image handle
     * @param imageMemory Output: allocated memory handle
     */
    void createImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                    VkFormat format, VkImageTiling tiling,
                    VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                    VkImage& image, VkDeviceMemory& imageMemory);

    /**
     * @brief Create an image view for a given image
     * @param image The image to create a view for
     * @param format Image format
     * @param mipLevels Number of mipmap levels (default: 1)
     * @return Created image view handle
     */
    VkImageView createImageView(VkImage image, VkFormat format, uint32_t mipLevels = 1);

    /**
     * @brief Transition an image from one layout to another
     * @param image The image to transition
     * @param format Image format
     * @param oldLayout Current layout
     * @param newLayout Target layout
     *
     * Uses a pipeline barrier to synchronize the transition.
     * Automatically infers appropriate access masks and pipeline stages.
     */
    void transitionImageLayout(VkImage image, VkFormat format,
                              VkImageLayout oldLayout, VkImageLayout newLayout);

    /**
     * @brief Copy buffer data to an image
     * @param buffer Source buffer
     * @param image Destination image
     * @param width Image width
     * @param height Image height
     *
     * Image must be in TRANSFER_DST_OPTIMAL layout before calling.
     */
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

private:
    VulkanContext* context = nullptr;
    VulkanCommand* command = nullptr;

    /**
     * @brief Find a suitable memory type for allocation
     * @param typeFilter Bit field of suitable memory types
     * @param properties Required memory properties
     * @return Index of suitable memory type
     */
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};
