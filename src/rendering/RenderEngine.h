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
#include "scene/Light.h"
#include "debug/ImGuiOverlay.h"
#include "gizmo/GizmoState.h"

class ShaderManager;
class DescriptorManager;
class ForwardPass;
class DebugRenderer;
class ShadowPass;

struct UniformBufferObject {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec3 viewPos;
    alignas(4) int lightCount;  // Number of active lights (0-8)
    alignas(4) float ambientStrength;
    alignas(4) float shininess;
    alignas(4) float metalness;  // PBR: 0 = dielectric, 1 = metal
    alignas(4) float roughness;  // PBR: 0 = smooth, 1 = rough
    GPULight lights[8];  // Array of lights (max 8)
    alignas(16) glm::mat4 lightSpaceMatrix;  // Light space transform for shadow mapping (first directional light)
    alignas(16) glm::ivec4 pointShadowIndicesA;  // Cube map index for GPU lights 0-3 (-1 = no cube shadow)
    alignas(16) glm::ivec4 pointShadowIndicesB;  // Cube map index for GPU lights 4-7 (-1 = no cube shadow)
};

// Push constants for per-object data (model matrix + material index)
struct PushConstantObject {
    alignas(16) glm::mat4 model;
    alignas(4) int materialIndex;  // Index into material buffer (for PBR shaders)
};

// Primitive types for procedural mesh generation
enum class PrimitiveType {
    Sphere,
    Cube,
    Plane
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

    // glTF integration helpers
    std::vector<int> loadTextures(const std::vector<std::string>& paths);
    std::vector<int> uploadMaterials(const std::vector<struct GLTFMaterial>& materials,
                                     const std::vector<int>& textureIndices);
    SceneObject createSceneObjectFromGLTF(const struct GLTFMeshData& gltfMesh,
                                          const std::vector<int>& materialIndices);
    void addSceneObject(SceneObject&& obj);
    void setPBRShader();

    // Window resize handling
    void setFramebufferResized(bool resized) { framebufferResized = resized; }

    // Camera access (for input handling from main.cpp)
    Camera& getCamera() { return scene.camera; }

    // Shadow pass access (for debug visualization in ImGui)
    ShadowPass* getShadowPass() { return shadowPass.get(); }

    // Scene management
    Scene& getScene() { return scene; }
    int getSelectedObjectIndex() const { return selectedObjectIndex; }
    void setSelectedObjectIndex(int index) { selectedObjectIndex = index; }
    void deleteObject(size_t index);

    // Procedural primitive generation
    void addPrimitive(PrimitiveType type,
                      const std::string& name,
                      const glm::vec3& position = glm::vec3(0.0f, 0.0f, 0.0f));

    // Gizmo management
    GizmoState& getGizmoState() { return gizmoState; }
    void setGizmoMode(GizmoMode mode) { gizmoState.mode = mode; }

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
    void updatePBRDescriptorSet();  // Update Set 1 with material buffer + texture array

    void createTextureImage();  // Legacy - for single texture meshes
    void createTextureImageView();  // Legacy
    void createTextureSampler();
    void generateMipmaps(VkImage image, VkFormat imageFormat,
                         int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

    // Multi-material texture loading (per-object)
    void loadMaterialTextures(SceneObject& object);
    void createMaterialDescriptorSets(SceneObject& object);

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
    std::unique_ptr<ShadowPass> shadowPass;

    // PBR asset managers
    std::unique_ptr<class TextureManager> textureManager;
    std::unique_ptr<class MaterialManager> materialManager;

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
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;  // Set 0: Global UBO
    VkDescriptorSetLayout pbrDescriptorSetLayout = VK_NULL_HANDLE;  // Set 1: Material SSBO + Texture Array
    std::vector<VkDescriptorSet> descriptorSets;  // Set 0 (per-frame)
    std::vector<VkDescriptorSet> pbrDescriptorSets;  // Set 1 (shared across frames)

    // Uniform Buffers
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;

    // Texture Resources (Legacy - for single-material meshes without textures)
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
    int selectedObjectIndex = -1;  // Index of selected object (-1 = none)

    // Gizmo
    GizmoState gizmoState;

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
