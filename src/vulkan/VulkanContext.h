#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <optional>
#include <vector>

// Helper struct for queue family indices (from Renderer.h)
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

/**
 * @brief Manages Vulkan core resources: Instance, Device, Queues, Surface
 *
 * Encapsulates all low-level Vulkan initialization:
 * - Instance creation with validation layers
 * - Physical device selection (GPU)
 * - Logical device creation
 * - Queue retrieval (Graphics + Present)
 * - Window surface creation
 */
class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext() = default;

    // No copy (Vulkan handles are not copyable)
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    /**
     * @brief Initialize Vulkan context
     * @param window GLFW window for surface creation
     * @param enableValidation Enable Vulkan validation layers (for debugging)
     */
    void init(GLFWwindow* window, bool enableValidation);

    /**
     * @brief Cleanup all Vulkan resources
     */
    void cleanup();

    // Getters (const - read-only access to internal resources)
    VkInstance getInstance() const { return instance; }
    VkDevice getDevice() const { return device; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    VkQueue getGraphicsQueue() const { return graphicsQueue; }
    VkQueue getPresentQueue() const { return presentQueue; }
    VkSurfaceKHR getSurface() const { return surface; }
    uint32_t getGraphicsQueueFamily() const { return graphicsQueueFamily; }
    uint32_t getPresentQueueFamily() const { return presentQueueFamily; }

private:
    // Vulkan core resources
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    // Queue family indices (stored for later use)
    uint32_t graphicsQueueFamily = 0;
    uint32_t presentQueueFamily = 0;

    // Configuration
    GLFWwindow* window = nullptr;
    bool validationEnabled = false;

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    // Initialization steps (called by init())
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();

    // Helper functions
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
};
