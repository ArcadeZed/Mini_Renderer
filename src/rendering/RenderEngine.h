#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <vector>
#include <string>
#include <memory>
#include <ctime>

#include "core/VulkanContext.h"
#include "core/VulkanSwapchain.h"
#include "core/VulkanCommand.h"
#include "core/VulkanResource.h"
#include "scene/Scene.h"
#include "debug/ImGuiOverlay.h"

class ShaderManager;
class DescriptorManager;
class ForwardPass;
class DebugRenderer;

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec3 lightPos;
    alignas(16) glm::vec3 lightColor;
    alignas(16) glm::vec3 viewPos;
    alignas(4) float ambientStrength;
    alignas(4) float shininess;
    alignas(4) float lightIntensity;
    alignas(4) float attenuationConstant;
    alignas(4) float attenuationLinear;
    alignas(4) float attenuationQuadratic;
};

class RenderEngine {
public:
    RenderEngine();
    ~RenderEngine();

    void init(GLFWwindow* window);
    void drawFrame();
    void cleanup();
    void checkAndReloadMesh();

private:
    void initVulkan();
    void createRenderPass();
    void createDepthResources();
    void createFramebuffers();
    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
                                 VkImageTiling tiling, VkFormatFeatureFlags features);
    bool hasStencilComponent(VkFormat format);

    void createDescriptorSetLayout();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void updateUniformBuffer(uint32_t currentImage);

    void createTextureImage();
    void createTextureImageView();
    void createTextureSampler();
    void generateMipmaps(VkImage image, VkFormat imageFormat,
                         int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

    void createSyncObjects();
    void recreateBuffers();
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);

    // Window
    GLFWwindow* window = nullptr;

    // Vulkan core modules
    VulkanContext context;
    VulkanSwapchain swapchain;
    VulkanCommand command;
    VulkanResource resource;
    std::unique_ptr<ShaderManager> shaderManager;
    std::unique_ptr<DescriptorManager> descriptorManager;

    // Render sub-systems
    std::unique_ptr<ForwardPass> forwardPass;
    std::unique_ptr<DebugRenderer> debugRenderer;
    std::unique_ptr<ImGuiOverlay> imguiOverlay;

    // ImGui interactive parameters
    ImGuiParams imguiParams;

    // Render pass & framebuffers
    VkRenderPass renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> swapchainFramebuffers;

    // Depth buffer
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;

    // Descriptors
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;

    // Uniform Buffers
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;

    // Texture Resources
    VkImage textureImage = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
    VkImageView textureImageView = VK_NULL_HANDLE;
    VkSampler textureSampler = VK_NULL_HANDLE;
    uint32_t mipLevels = 1;

    // Command buffers (allocated once, re-recorded each frame)
    std::vector<VkCommandBuffer> commandBuffers;

    // Sync objects
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    size_t currentFrame = 0;

    // Scene
    Scene scene;

    // Hot-reload state
    std::string meshFilePath;
    std::time_t lastFileModTime = 0;

    // Constants
    const int MAX_FRAMES_IN_FLIGHT = 1;

#ifdef FORCE_ENABLE_VALIDATION_LAYERS
    const bool enableValidationLayers = true;
#else
    const bool enableValidationLayers = false;
#endif
};
