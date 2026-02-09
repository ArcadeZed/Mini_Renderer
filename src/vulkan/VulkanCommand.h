#pragma once

#include <vulkan/vulkan.h>
#include <vector>

// Forward declaration
class VulkanContext;

/**
 * @brief Manages Vulkan Command Pool and Command Buffer allocation
 *
 * Provides command pool management and helpers for:
 * - Command buffer allocation (does NOT record them - that's Renderer's job!)
 * - Single-time commands for buffer/image operations
 *
 * Recording command buffers remains the responsibility of the Renderer.
 */
class VulkanCommand {
public:
    VulkanCommand() = default;
    ~VulkanCommand() = default;

    // No copy (Vulkan handles are not copyable)
    VulkanCommand(const VulkanCommand&) = delete;
    VulkanCommand& operator=(const VulkanCommand&) = delete;

    /**
     * @brief Initialize command pool
     * @param ctx Pointer to initialized VulkanContext
     */
    void init(VulkanContext* ctx);

    /**
     * @brief Cleanup command pool (also frees all allocated command buffers)
     */
    void cleanup();

    /**
     * @brief Allocate command buffers from the pool
     * @param outBuffers Output vector to store allocated command buffers
     * @param count Number of command buffers to allocate
     *
     * Note: This only allocates buffers, does NOT record them!
     * The caller (Renderer) is responsible for recording draw commands.
     */
    void allocateCommandBuffers(std::vector<VkCommandBuffer>& outBuffers, uint32_t count);

    /**
     * @brief Begin a single-time command buffer (for immediate operations)
     * @return Temporary command buffer ready for recording
     *
     * Used for one-off operations like buffer copies or image layout transitions.
     * Must be ended with endSingleTimeCommands().
     */
    VkCommandBuffer beginSingleTimeCommands();

    /**
     * @brief End and submit a single-time command buffer
     * @param commandBuffer The command buffer started with beginSingleTimeCommands()
     *
     * Submits the command buffer, waits for completion, and frees it.
     */
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    // Getter (const - read-only access)
    VkCommandPool getCommandPool() const { return commandPool; }

private:
    VulkanContext* context = nullptr;
    VkCommandPool commandPool = VK_NULL_HANDLE;

    void createCommandPool();
};
