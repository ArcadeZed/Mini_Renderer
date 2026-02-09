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
    windowHandle = window;

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

    // 3. Modern UE5-style theme
    setupModernStyle();

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

    std::cout << "ImGuiOverlay: Initialized with modern UE5-style theme." << std::endl;
}

void ImGuiOverlay::beginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiOverlay::buildUI(ImGuiParams& params) {
    // Get window dimensions for right-side anchoring
    int windowWidth, windowHeight;
    glfwGetWindowSize(windowHandle, &windowWidth, &windowHeight);

    const float panelWidth = 350.0f;
    const float panelHeight = static_cast<float>(windowHeight);
    const float padding = 10.0f;

    // Anchor panel to the right side
    ImGui::SetNextWindowPos(ImVec2(windowWidth - panelWidth - padding, padding), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight - 2.0f * padding), ImGuiCond_Always);

    // Create window with no resizing, no moving, no collapsing
    ImGui::Begin("Renderer Settings", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    // Lighting Model Selection
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    if (ImGui::CollapsingHeader("Lighting Model", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PopStyleColor();
        const char* items[] = { "Phong", "Blinn-Phong", "PBR (Cook-Torrance)" };
        ImGui::Combo("Model", &params.lightingModelIndex, items, IM_ARRAYSIZE(items));
        ImGui::TextWrapped("Switch between different lighting models at runtime.");
    } else {
        ImGui::PopStyleColor();
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PopStyleColor();
        ImGui::DragFloat3("Position", &params.lightPos.x, 0.5f);
        ImGui::ColorEdit3("Color", &params.lightColor.x);
        ImGui::SliderFloat("Intensity", &params.lightIntensity, 0.0f, 1000.0f);
    } else {
        ImGui::PopStyleColor();
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    if (ImGui::CollapsingHeader("Attenuation")) {
        ImGui::PopStyleColor();
        ImGui::SliderFloat("Constant", &params.attenuationConstant, 0.0f, 5.0f);
        ImGui::SliderFloat("Linear", &params.attenuationLinear, 0.0f, 0.5f);
        ImGui::SliderFloat("Quadratic", &params.attenuationQuadratic, 0.0f, 0.1f);
    } else {
        ImGui::PopStyleColor();
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PopStyleColor();
        // Phong/Blinn-Phong parameters
        if (params.lightingModelIndex <= 1) {
            ImGui::SliderFloat("Ambient", &params.ambientStrength, 0.0f, 1.0f);
            ImGui::SliderFloat("Shininess", &params.shininess, 1.0f, 256.0f);
        }

        // PBR parameters
        if (params.lightingModelIndex == 2) {
            ImGui::SliderFloat("Metalness", &params.metalness, 0.0f, 1.0f);
            ImGui::SliderFloat("Roughness", &params.roughness, 0.01f, 1.0f);
            ImGui::TextWrapped("Metalness: 0=Plastic/Wood, 1=Gold/Iron");
            ImGui::TextWrapped("Roughness: 0=Mirror, 1=Matte");
        }
    } else {
        ImGui::PopStyleColor();
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    if (ImGui::CollapsingHeader("Debug")) {
        ImGui::PopStyleColor();
        ImGui::Checkbox("Show Axes", &params.showDebugAxes);
        ImGui::Checkbox("Show Grid", &params.showGrid);
    } else {
        ImGui::PopStyleColor();
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

void ImGuiOverlay::setupModernStyle() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Rounding for modern look (UE5-style)
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 3.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.PopupRounding = 4.0f;

    // Spacing and padding
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.FramePadding = ImVec2(8.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;

    // Borders
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;

    // UE5-inspired dark color scheme (almost black with blue/orange accents)
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                   = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.10f, 0.10f, 0.11f, 0.96f);  // Very dark grey
    colors[ImGuiCol_ChildBg]                = ImVec4(0.12f, 0.12f, 0.13f, 1.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.10f, 0.10f, 0.11f, 0.98f);
    colors[ImGuiCol_Border]                 = ImVec4(0.25f, 0.25f, 0.27f, 0.60f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);  // Input fields
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.22f, 0.22f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.28f, 0.28f, 0.30f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.12f, 0.12f, 0.13f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.08f, 0.08f, 0.09f, 0.75f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.12f, 0.12f, 0.13f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.10f, 0.10f, 0.11f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.30f, 0.30f, 0.32f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.40f, 0.40f, 0.42f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.50f, 0.50f, 0.52f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.35f, 0.60f, 0.95f, 1.00f);  // Blue accent
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.35f, 0.60f, 0.95f, 0.80f);  // Blue accent
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.40f, 0.68f, 1.00f, 1.00f);  // Bright blue
    colors[ImGuiCol_Button]                 = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.35f, 0.60f, 0.95f, 0.60f);  // Blue hover
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.35f, 0.60f, 0.95f, 1.00f);  // Blue active
    colors[ImGuiCol_Header]                 = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);  // Headers darker
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.35f, 0.60f, 0.95f, 0.50f);  // Blue on hover
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.35f, 0.60f, 0.95f, 0.80f);
    colors[ImGuiCol_Separator]              = ImVec4(0.25f, 0.25f, 0.27f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.35f, 0.60f, 0.95f, 0.60f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.35f, 0.60f, 0.95f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.35f, 0.60f, 0.95f, 0.30f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.35f, 0.60f, 0.95f, 0.70f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.35f, 0.60f, 0.95f, 1.00f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.35f, 0.60f, 0.95f, 0.60f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.28f, 0.50f, 0.85f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.12f, 0.14f, 0.17f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(0.35f, 0.60f, 0.95f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);  // Orange accent
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.80f, 0.20f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.35f, 0.60f, 0.95f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(0.35f, 0.60f, 0.95f, 0.90f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.35f, 0.60f, 0.95f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.10f, 0.10f, 0.11f, 0.75f);
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
