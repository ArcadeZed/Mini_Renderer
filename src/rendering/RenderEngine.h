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
    alignas(4) float metalness;  // PBR: 0 = dielectric, 1 = metal
    alignas(4) float roughness;  // PBR: 0 = smooth, 1 = rough
};

class RenderEngine {
public:
    RenderEngine();
    ~RenderEngine();

    void init(GLFWwindow* window,
              const std::string& meshPath = "",
              const std::string& texturePath = "");
    void drawFrame();
    void cleanup();
    void checkAndReloadMesh();

    // Dynamic loading at runtime
    void loadMesh(const std::string& filepath);
    void loadTexture(const std::string& filepath);

    // Window resize handling
    void setFramebufferResized(bool resized) { framebufferResized = resized; }

    // Camera access (for input handling from main.cpp)
    Camera& getCamera() { return scene.camera; }

private:
    void initVulkan();
    void createRenderPass();
    void createDepthResources();
    void createFramebuffers();
    void recreateSwapchain();
    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
                                 VkImageTiling tiling, VkFormatFeatureFlags features);
    bool hasStencilComponent(VkFormat format);

    void createDescriptorSetLayout();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void updateUniformBuffer(uint32_t currentImage);

    void createTextureImage();  // Legacy - for single texture meshes
    void createTextureImageView();  // Legacy
    void createTextureSampler();
    void generateMipmaps(VkImage image, VkFormat imageFormat,
                         int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

    // Multi-material texture loading
    void loadMaterialTextures();
    void createMaterialDescriptorSets();
    void cleanupMaterialResources();

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
    int previousLightingModelIndex = 0;  // Track lighting model changes

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

    // Texture Resources (Legacy - for single-material meshes)
    VkImage textureImage = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
    VkImageView textureImageView = VK_NULL_HANDLE;
    VkSampler textureSampler = VK_NULL_HANDLE;
    uint32_t mipLevels = 1;

    // Multi-material Resources (NEW)
    struct MaterialResources {
        VkImage textureImage = VK_NULL_HANDLE;
        VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
        VkImageView textureImageView = VK_NULL_HANDLE;
        uint32_t mipLevels = 1;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;  // Descriptor set for this material
    };
    std::vector<MaterialResources> materialResources;  // One per material in mesh
    bool useMultiMaterial = false;  // Flag: true if mesh has multiple materials

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
    std::string textureFilePath;
    std::time_t lastFileModTime = 0;

    // Window resize state
    bool framebufferResized = false;

    // Constants
    const int MAX_FRAMES_IN_FLIGHT = 1;

#ifdef FORCE_ENABLE_VALIDATION_LAYERS
    const bool enableValidationLayers = true;
#else
    const bool enableValidationLayers = false;
#endif
};
