#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <vector>

// Forward declaration
class VulkanContext;

// Swapchain support details
struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

/**
 * @brief Manages Vulkan Swapchain: presentation images and image views
 *
 * Encapsulates swapchain creation, image view creation, and format selection.
 * Depends on VulkanContext for device, surface, and queue access.
 */
class VulkanSwapchain {
public:
    VulkanSwapchain() = default;
    ~VulkanSwapchain() = default;

    // No copy (Vulkan handles are not copyable)
    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

    /**
     * @brief Initialize swapchain and image views
     * @param ctx Pointer to initialized VulkanContext
     * @param win GLFW window for framebuffer size queries
     */
    void init(VulkanContext* ctx, GLFWwindow* win);

    /**
     * @brief Cleanup swapchain resources (image views, swapchain)
     */
    void cleanup();

    // Getters (const - read-only access to internal resources)
    VkSwapchainKHR getSwapchain() const { return swapchain; }
    const std::vector<VkImage>& getImages() const { return swapchainImages; }
    VkFormat getImageFormat() const { return swapchainImageFormat; }
    VkExtent2D getExtent() const { return swapchainExtent; }
    const std::vector<VkImageView>& getImageViews() const { return swapchainImageViews; }

private:
    // Dependencies
    VulkanContext* context = nullptr;
    GLFWwindow* window = nullptr;

    // Swapchain resources
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages;
    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;
    std::vector<VkImageView> swapchainImageViews;

    // Initialization steps (called by init())
    void createSwapchain();
    void createImageViews();

    // Helper functions
    SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
};
