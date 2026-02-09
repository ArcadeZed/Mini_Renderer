#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

class VulkanContext;

struct ImGuiParams {
    // Light
    glm::vec3 lightPos{10.0f, 100.0f, -10.0f};
    glm::vec3 lightColor{1.0f, 1.0f, 1.0f};
    float lightIntensity = 200.0f;

    // Attenuation
    float attenuationConstant = 1.0f;
    float attenuationLinear = 0.045f;
    float attenuationQuadratic = 0.0075f;

    // Material
    float ambientStrength = 0.1f;
    float shininess = 32.0f;

    // Debug visualization toggles
    bool showDebugAxes = true;
    bool showGrid = true;
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
    void buildUI(ImGuiParams& params);
    void record(VkCommandBuffer cmd);
    void cleanup(VkDevice device);

private:
    VkDescriptorPool imguiPool = VK_NULL_HANDLE;
};
