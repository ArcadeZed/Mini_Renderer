#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

class ShaderManager;
class VulkanResource;
class VulkanCommand;
class Scene;
struct Light;

static constexpr int MAX_POINT_SHADOWS = 4;

// Push constant for cube shadow pass (point lights)
struct CubeShadowPushConstant {
    glm::mat4 faceVP;              // 64 bytes — per-face view-projection
    glm::mat4 model;               // 64 bytes — object model matrix
    glm::vec4 lightPosAndFarPlane; // 16 bytes — xyz=lightPos, w=farPlane
};  // Total: 144 bytes

// Shadow pass: Renders scene from light's perspective to generate depth map
class ShadowPass {
public:
    ShadowPass() = default;
    ~ShadowPass() = default;

    ShadowPass(const ShadowPass&) = delete;
    ShadowPass& operator=(const ShadowPass&) = delete;

    void init(VkDevice device,
              VulkanResource& resource,
              VulkanCommand& command,
              ShaderManager& shaderManager,
              uint32_t shadowMapSize = 1024);

    // Directional light shadow (2D depth map)
    void record(VkCommandBuffer cmd,
                const Scene& scene,
                const Light& light);

    // Point light shadow (cube map, 6 faces)
    // cubeIndex selects which cube map slot (0 to MAX_POINT_SHADOWS-1)
    void recordCube(VkCommandBuffer cmd,
                    const Scene& scene,
                    const Light& light,
                    int cubeIndex);

    void cleanup(VkDevice device);

    // Directional shadow map getters
    VkImageView getShadowMapView() const { return shadowMapView; }
    VkSampler getShadowMapSampler() const { return shadowMapSampler; }
    VkImageView getShadowMapPreviewView() const { return shadowMapPreviewView; }

    // Cube shadow map getters (indexed by cube slot)
    VkImageView getCubeShadowMapView(int cubeIndex) const { return cubeShadowMapViews[cubeIndex]; }
    VkSampler getCubeShadowMapSampler() const { return cubeShadowMapSampler; }
    VkImageView getDummyCubeView() const { return dummyCubeView; }
    VkImageView getCubeFacePreviewView(int cubeIndex, int face) const { return cubeFacePreviewViews[cubeIndex][face]; }
    int getMaxPointShadows() const { return MAX_POINT_SHADOWS; }

private:
    // Directional shadow setup
    void createShadowResources();
    void createShadowRenderPass();
    void createShadowFramebuffer();
    void createShadowPipeline();
    void createShadowSampler();

    // Cube shadow setup (point lights)
    void createCubeShadowResources();
    void createCubeShadowFramebuffers();
    void createCubeShadowPipeline();
    void createCubeShadowSampler();
    void createDummyCubeTexture();

    VkDevice device = VK_NULL_HANDLE;
    VulkanResource* resource = nullptr;
    VulkanCommand* command = nullptr;
    ShaderManager* shaderManager = nullptr;

    uint32_t shadowMapSize = 1024;

    // Directional shadow map resources
    VkImage shadowMapImage = VK_NULL_HANDLE;
    VkDeviceMemory shadowMapMemory = VK_NULL_HANDLE;
    VkImageView shadowMapView = VK_NULL_HANDLE;
    VkImageView shadowMapPreviewView = VK_NULL_HANDLE;
    VkSampler shadowMapSampler = VK_NULL_HANDLE;

    // Shadow render pass and framebuffer (shared for directional + cube faces)
    VkRenderPass shadowRenderPass = VK_NULL_HANDLE;
    VkFramebuffer shadowFramebuffer = VK_NULL_HANDLE;

    // Directional shadow pipeline
    VkPipeline shadowPipeline = VK_NULL_HANDLE;
    VkPipelineLayout shadowPipelineLayout = VK_NULL_HANDLE;

    // Cube shadow map resources (point lights) — MAX_POINT_SHADOWS slots
    uint32_t cubeMapSize = 1024;
    VkImage cubeShadowMapImages[MAX_POINT_SHADOWS] = {};
    VkDeviceMemory cubeShadowMapMemories[MAX_POINT_SHADOWS] = {};
    VkImageView cubeShadowMapViews[MAX_POINT_SHADOWS] = {};                  // CUBE views for sampling
    VkImageView cubeFaceViews[MAX_POINT_SHADOWS][6] = {};                    // Per-face 2D views (framebuffer)
    VkImageView cubeFacePreviewViews[MAX_POINT_SHADOWS][6] = {};             // Per-face R→RGB swizzle (ImGui)
    VkSampler cubeShadowMapSampler = VK_NULL_HANDLE;
    VkFramebuffer cubeFaceFramebuffers[MAX_POINT_SHADOWS][6] = {};

    // Cube shadow pipeline (has fragment shader for linear depth)
    VkPipeline cubeShadowPipeline = VK_NULL_HANDLE;
    VkPipelineLayout cubeShadowPipelineLayout = VK_NULL_HANDLE;

    // Dummy 1×1×6 cube (bound to binding 3 when no point shadow active)
    VkImage dummyCubeImage = VK_NULL_HANDLE;
    VkDeviceMemory dummyCubeMemory = VK_NULL_HANDLE;
    VkImageView dummyCubeView = VK_NULL_HANDLE;
};
