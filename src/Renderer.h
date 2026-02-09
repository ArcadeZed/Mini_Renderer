#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <vulkan/vulkan.h>
#include <array>

#include <vector>
#include <string> // Für Shader-Pfade
#include <optional>
#include <ctime>  // Für std::time_t
#include <glm/gtc/matrix_transform.hpp>

#include "vulkan/VulkanContext.h"
#include "vulkan/VulkanSwapchain.h"

// Vertex-Struktur für unser Dreieck
struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec3 normal;
    glm::vec2 uv;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {  // 4 statt 3!
        std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

        // Position (location = 0)
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        // Color (location = 1)
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        // Normal (location = 2)
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, normal);

        // UV (location = 3)
        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(Vertex, uv);

        return attributeDescriptions;
    }
};

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec3 lightPos;
    alignas(16) glm::vec3 lightColor;
    alignas(16) glm::vec3 viewPos;  // Camera position
    alignas(4) float ambientStrength;
    alignas(4) float shininess;
    alignas(4) float lightIntensity;
    alignas(4) float attenuationConstant;
    alignas(4) float attenuationLinear;
    alignas(4) float attenuationQuadratic;
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    void init(GLFWwindow* window);
    void drawFrame();
    void cleanup();
    void checkAndReloadMesh();

private:
    void initVulkan();
    void createRenderPass();
    void createDepthResources();
    void createGraphicsPipeline();
    void createDebugPipeline();
    void createInfiniteGridPipeline();
    void createFramebuffers();
    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    bool hasStencilComponent(VkFormat format);
    void createCommandPool();
    void createVertexBuffer();
    void createIndexBuffer();
    void createCommandBuffers();
    void createSyncObjects();

    // Descriptor Set Layout & Pool
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    // Uniform Buffers
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;

    // Texture Resources (NEU!)
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
    VkSampler textureSampler;
    uint32_t mipLevels;

    void createDescriptorSetLayout();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void updateUniformBuffer(uint32_t currentImage);

    void createTextureImage();
    void createTextureImageView();
    void createTextureSampler();
    void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);


    // Helper functions
    void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling,
                VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                VkImage& image, VkDeviceMemory& imageMemory);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    VkImageView createImageView(VkImage image, VkFormat format, uint32_t mipLevels = 1);

    // Helper Functions
    VkShaderModule createShaderModule(const std::vector<char>& code);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void loadMeshFromFile(const std::string& filename);
    void recreateBuffers();
    void createDebugGeometry();
    void createDebugBuffers();

    // Mesh Data (loaded from file - Hot Reloadable)
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    std::string meshFilePath;
    std::time_t lastFileModTime;

    // Debug Geometry (Grid + Axes - permanent, unchanging)
    std::vector<Vertex> debugVertices;
    std::vector<uint16_t> debugIndices;
    VkBuffer debugVertexBuffer;
    VkDeviceMemory debugVertexBufferMemory;
    VkBuffer debugIndexBuffer;
    VkDeviceMemory debugIndexBufferMemory;

    // Member-Vars
    GLFWwindow* window;

    // Vulkan Core (Instance, Device, Queues, Surface)
    VulkanContext context;

    // Vulkan Swapchain (Presentation images & image views)
    VulkanSwapchain swapchain;

    std::vector<VkFramebuffer> swapchainFramebuffers;

    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;

    // Debug rendering pipeline (unlit, no texture)
    VkPipelineLayout debugPipelineLayout;
    VkPipeline debugPipeline;

    // Infinite grid pipeline (procedural grid in shader)
    VkPipelineLayout gridPipelineLayout;
    VkPipeline gridPipeline;

    // Depth buffer resources
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    // Synchronisationsobjekte
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight; // Um zu verfolgen, welche Bilder bereits in Bearbeitung sind
    size_t currentFrame = 0;

    // Konstanten und Hilfsfelder
    const std::vector<const char*> validationLayers = {
            "VK_LAYER_KHRONOS_validation"
    };
    const std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

// Validation Layers können über CMake gesteuert werden: -DENABLE_VALIDATION_LAYERS=ON/OFF
#ifdef FORCE_ENABLE_VALIDATION_LAYERS
    const bool enableValidationLayers = true;
#else
    const bool enableValidationLayers = false;  // Standard: AUS (wegen OBS/Overwolf Konflikt)
#endif

    // Anzahl der gleichzeitig in Flug befindlichen Frames
    const int MAX_FRAMES_IN_FLIGHT = 1;
};