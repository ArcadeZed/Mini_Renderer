#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <filesystem>
#include <vector>

// FontAwesome icons
#include "../../external/fonts/IconsFontAwesome6.h"

class VulkanContext;

struct ImGuiParams {
    // Lighting Model selection (0=Phong, 1=Blinn-Phong, 2=PBR)
    int lightingModelIndex = 0;

    // Light
    glm::vec3 lightPos{10.0f, 100.0f, -10.0f};
    glm::vec3 lightColor{1.0f, 1.0f, 1.0f};
    float lightIntensity = 200.0f;

    // Attenuation
    float attenuationConstant = 1.0f;
    float attenuationLinear = 0.045f;
    float attenuationQuadratic = 0.0075f;

    // Material (Phong/Blinn-Phong)
    float ambientStrength = 0.1f;
    float shininess = 32.0f;

    // PBR Material
    float metalness = 0.0f;   // 0 = dielectric, 1 = metal
    float roughness = 0.5f;   // 0 = smooth, 1 = rough

    // Debug visualization toggles
    bool showDebugAxes = true;
    bool showGrid = true;
    bool showLightGizmos = true;
};

class ImGuiOverlay {
public:
    ImGuiOverlay() = default;
    ~ImGuiOverlay() = default;

    ImGuiOverlay(const ImGuiOverlay&) = delete;
    ImGuiOverlay& operator=(const ImGuiOverlay&) = delete;

    void init(GLFWwindow* window,
              VulkanContext& context,
              VkRenderPass renderPass,
              uint32_t imageCount);

    void beginFrame();
    void buildUI(ImGuiParams& params, class Scene& scene, int& selectedObjectIndex, struct GizmoState& gizmoState, class RenderEngine* renderEngine = nullptr);
    void buildContentBrowser(class RenderEngine* renderEngine = nullptr);
    void record(VkCommandBuffer cmd);
    void cleanup(VkDevice device);

private:
    void setupModernStyle();
    void refreshCurrentDirectory();
    void navigateToDirectory(const std::filesystem::path& path);
    void buildSceneManagerWindow(class Scene& scene,
                                  int& selectedObjectIndex,
                                  struct GizmoState& gizmoState,
                                  class RenderEngine* renderEngine);

    VkDescriptorPool imguiPool = VK_NULL_HANDLE;
    GLFWwindow* windowHandle = nullptr;

    // Content Browser state
    std::filesystem::path rootPath;
    std::filesystem::path currentPath;
    std::vector<std::filesystem::directory_entry> currentDirectories;
    std::vector<std::filesystem::directory_entry> currentFiles;
    bool contentBrowserInitialized = false;
};
