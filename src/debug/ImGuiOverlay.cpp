#include "ImGuiOverlay.h"
#include "../core/VulkanContext.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <stdexcept>
#include <iostream>

void ImGuiOverlay::init(GLFWwindow* window,
                        VulkanContext& context,
                        VkRenderPass renderPass,
                        uint32_t imageCount) {
    // 1. Create dedicated descriptor pool for ImGui
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 }
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 100;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = poolSizes;

    if (vkCreateDescriptorPool(context.getDevice(), &poolInfo, nullptr, &imguiPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui descriptor pool!");
    }

    // 2. Initialize ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // 3. Style
    ImGui::StyleColorsDark();

    // 4. Initialize GLFW backend
    ImGui_ImplGlfw_InitForVulkan(window, true);

    // 5. Initialize Vulkan backend (v1.92+ API)
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = context.getInstance();
    initInfo.PhysicalDevice = context.getPhysicalDevice();
    initInfo.Device = context.getDevice();
    initInfo.QueueFamily = context.getGraphicsQueueFamily();
    initInfo.Queue = context.getGraphicsQueue();
    initInfo.DescriptorPool = imguiPool;
    initInfo.MinImageCount = imageCount;
    initInfo.ImageCount = imageCount;
    initInfo.PipelineInfoMain.RenderPass = renderPass;
    initInfo.PipelineInfoMain.Subpass = 0;

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        throw std::runtime_error("Failed to initialize ImGui Vulkan backend!");
    }

    std::cout << "ImGuiOverlay: Initialized." << std::endl;
}

void ImGuiOverlay::beginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiOverlay::buildUI(ImGuiParams& params) {
    ImGui::Begin("Renderer Settings");

    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Position", &params.lightPos.x, 0.5f);
        ImGui::ColorEdit3("Color", &params.lightColor.x);
        ImGui::SliderFloat("Intensity", &params.lightIntensity, 0.0f, 1000.0f);
    }

    if (ImGui::CollapsingHeader("Attenuation")) {
        ImGui::SliderFloat("Constant", &params.attenuationConstant, 0.0f, 5.0f);
        ImGui::SliderFloat("Linear", &params.attenuationLinear, 0.0f, 0.5f);
        ImGui::SliderFloat("Quadratic", &params.attenuationQuadratic, 0.0f, 0.1f);
    }

    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Ambient", &params.ambientStrength, 0.0f, 1.0f);
        ImGui::SliderFloat("Shininess", &params.shininess, 1.0f, 256.0f);
    }

    if (ImGui::CollapsingHeader("Debug")) {
        ImGui::Checkbox("Show Axes", &params.showDebugAxes);
        ImGui::Checkbox("Show Grid", &params.showGrid);
    }

    ImGui::Separator();
    ImGui::Text("%.1f FPS (%.3f ms/frame)",
                ImGui::GetIO().Framerate,
                1000.0f / ImGui::GetIO().Framerate);

    ImGui::End();
}

void ImGuiOverlay::record(VkCommandBuffer cmd) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void ImGuiOverlay::cleanup(VkDevice device) {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (imguiPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, imguiPool, nullptr);
        imguiPool = VK_NULL_HANDLE;
    }

    std::cout << "ImGuiOverlay: Cleaned up." << std::endl;
}
