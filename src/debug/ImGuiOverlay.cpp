#include "ImGuiOverlay.h"
#include "../core/VulkanContext.h"
#include "../scene/Scene.h"
#include "../rendering/RenderEngine.h"
#include "../rendering/ShadowPass.h"
#include "../gizmo/GizmoState.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <stdexcept>
#include <iostream>
#include <filesystem>
#include <algorithm>

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

    // 3. Load fonts (default + FontAwesome icons)
    // Load default font
    io.Fonts->AddFontDefault();

    // Merge FontAwesome icons into default font
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.GlyphMinAdvanceX = 16.0f; // Monospace icons

    std::string fontPath = "external/fonts/fa-solid-900.ttf";
    if (std::filesystem::exists(fontPath)) {
        io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f, &icons_config, icons_ranges);
        std::cout << "ImGuiOverlay: FontAwesome loaded successfully." << std::endl;
    } else {
        std::cerr << "ImGuiOverlay: WARNING - FontAwesome font not found at: " << fontPath << std::endl;
        std::cerr << "             Content Browser will use fallback text icons." << std::endl;
    }

    // 4. Modern UE5-style theme
    setupModernStyle();

    // 5. Initialize GLFW backend
    ImGui_ImplGlfw_InitForVulkan(window, true);

    // 6. Initialize Vulkan backend (v1.92+ API)
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

void ImGuiOverlay::buildUI(ImGuiParams& params, class Scene& scene, int& selectedObjectIndex, struct GizmoState& gizmoState, class RenderEngine* renderEngine) {
    // NEW: Scene Manager window (left side)
    buildSceneManagerWindow(scene, selectedObjectIndex, gizmoState, renderEngine);

    // Get window dimensions for right-side anchoring
    int windowWidth, windowHeight;
    glfwGetWindowSize(windowHandle, &windowWidth, &windowHeight);

    const float panelWidth = 350.0f;
    const float panelHeight = static_cast<float>(windowHeight);
    const float padding = 10.0f;

    // Anchor panel to the right side (only on first use)
    ImGui::SetNextWindowPos(ImVec2(windowWidth - panelWidth - padding, padding), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight - 2.0f * padding), ImGuiCond_FirstUseEver);

    // Create window (freely movable, resizable, collapsible)
    ImGui::Begin("Renderer Settings", nullptr);

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
        ImGui::DragFloat3("Position##Light", &params.lightPos.x, 0.5f);
        ImGui::ColorEdit3("Color##Light", &params.lightColor.x);
        ImGui::SliderFloat("Intensity##Light", &params.lightIntensity, 0.0f, 1000.0f);
    } else {
        ImGui::PopStyleColor();
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    if (ImGui::CollapsingHeader("Attenuation")) {
        ImGui::PopStyleColor();
        ImGui::SliderFloat("Constant##Attenuation", &params.attenuationConstant, 0.0f, 5.0f);
        ImGui::SliderFloat("Linear##Attenuation", &params.attenuationLinear, 0.0f, 0.5f);
        ImGui::SliderFloat("Quadratic##Attenuation", &params.attenuationQuadratic, 0.0f, 0.1f);
    } else {
        ImGui::PopStyleColor();
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PopStyleColor();
        // Phong/Blinn-Phong parameters
        if (params.lightingModelIndex <= 1) {
            ImGui::SliderFloat("Ambient##Material", &params.ambientStrength, 0.0f, 1.0f);
            ImGui::SliderFloat("Shininess##Material", &params.shininess, 1.0f, 256.0f);
        }

        // PBR parameters
        if (params.lightingModelIndex == 2) {
            ImGui::SliderFloat("Metalness##Material", &params.metalness, 0.0f, 1.0f);
            ImGui::SliderFloat("Roughness##Material", &params.roughness, 0.01f, 1.0f);
            ImGui::TextWrapped("Metalness: 0=Plastic/Wood, 1=Gold/Iron");
            ImGui::TextWrapped("Roughness: 0=Mirror, 1=Matte");
        }
    } else {
        ImGui::PopStyleColor();
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PopStyleColor();

        // Camera position (read from camera, apply via setter)
        glm::vec3 camPos = scene.camera.getPosition();
        if (ImGui::DragFloat3("Position##Camera", &camPos.x, 0.5f)) {
            scene.camera.setPosition(camPos);
        }

        // Camera target
        glm::vec3 camTarget = scene.camera.getTarget();
        if (ImGui::DragFloat3("Target##Camera", &camTarget.x, 0.5f)) {
            scene.camera.setTarget(camTarget);
        }

        // FOV
        float fov = scene.camera.getFOV();
        if (ImGui::SliderFloat("FOV##Camera", &fov, 30.0f, 120.0f, "%.1f°")) {
            scene.camera.setFOV(fov);
        }

        ImGui::Separator();
        ImGui::Text("Clipping Planes:");

        // Near/Far planes (editable)
        float nearPlane = scene.camera.getNearPlane();
        float farPlane = scene.camera.getFarPlane();

        bool planesChanged = false;
        planesChanged |= ImGui::SliderFloat("Near Plane##Camera", &nearPlane, 0.01f, 10.0f, "%.3f");
        planesChanged |= ImGui::SliderFloat("Far Plane##Camera", &farPlane, 100.0f, 10000.0f, "%.1f");

        if (planesChanged) {
            scene.camera.setClipPlanes(nearPlane, farPlane);
        }

        ImGui::Separator();
        ImGui::Text("Movement Settings:");

        // Move speed
        float moveSpeed = scene.camera.getMoveSpeed();
        if (ImGui::SliderFloat("Move Speed##Camera", &moveSpeed, 1.0f, 200.0f, "%.1f u/s")) {
            scene.camera.setMoveSpeed(moveSpeed);
        }

        // Mouse sensitivity
        float mouseSens = scene.camera.getMouseSensitivity();
        if (ImGui::SliderFloat("Mouse Sensitivity##Camera", &mouseSens, 0.001f, 0.02f, "%.4f")) {
            scene.camera.setMouseSensitivity(mouseSens);
        }

        // Scroll speed
        float scrollSpeed = scene.camera.getScrollSpeed();
        if (ImGui::SliderFloat("Scroll Speed##Camera", &scrollSpeed, 1.0f, 20.0f, "%.1f")) {
            scene.camera.setScrollSpeed(scrollSpeed);
        }

        ImGui::Separator();
        ImGui::TextWrapped("Right Mouse: Fly | Alt+Left: Orbit | Middle: Pan | Scroll: Zoom");
    } else {
        ImGui::PopStyleColor();
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    if (ImGui::CollapsingHeader("Debug")) {
        ImGui::PopStyleColor();
        ImGui::Checkbox("Show Axes", &params.showDebugAxes);
        ImGui::Checkbox("Show Grid", &params.showGrid);
        ImGui::Checkbox("Show Light Gizmos", &params.showLightGizmos);
    } else {
        ImGui::PopStyleColor();
    }

    ImGui::Separator();
    ImGui::Text("%.1f FPS (%.3f ms/frame)",
                ImGui::GetIO().Framerate,
                1000.0f / ImGui::GetIO().Framerate);

    ImGui::End();

    // Content Browser (full file manager)
    buildContentBrowser(renderEngine);
}

void ImGuiOverlay::buildSceneManagerWindow(Scene& scene,
                                            int& selectedObjectIndex,
                                            GizmoState& gizmoState,
                                            RenderEngine* renderEngine) {
    // Window setup (left side, 300px width)
    ImGui::SetNextWindowPos(ImVec2(10, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 600), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Scene Manager", nullptr, ImGuiWindowFlags_None)) {
        ImGui::End();
        return;
    }

    // ======= SECTION 1: Add Primitive Buttons =======
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    ImGui::SeparatorText("Add Primitive");
    ImGui::PopStyleColor();

    if (renderEngine) {
        // Counter for unique naming
        static int sphereCount = 0;
        static int cubeCount = 0;
        static int planeCount = 0;

        // Sphere button
        if (ImGui::Button(ICON_FA_CIRCLE_PLUS " Sphere", ImVec2(-1, 0))) {
            std::string name = "Sphere_" + std::to_string(++sphereCount);
            renderEngine->addPrimitive(PrimitiveType::Sphere, name);
        }

        // Cube button
        if (ImGui::Button(ICON_FA_CUBE " Cube", ImVec2(-1, 0))) {
            std::string name = "Cube_" + std::to_string(++cubeCount);
            renderEngine->addPrimitive(PrimitiveType::Cube, name);
        }

        // Plane button
        if (ImGui::Button(ICON_FA_IMAGE " Plane", ImVec2(-1, 0))) {
            std::string name = "Plane_" + std::to_string(++planeCount);
            renderEngine->addPrimitive(PrimitiveType::Plane, name);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ======= SECTION 2: Scene Hierarchy (moved from Renderer Settings) =======
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    if (ImGui::CollapsingHeader("Scene Hierarchy", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PopStyleColor();

        ImGui::Text("Objects: %zu", scene.objects.size());
        ImGui::Separator();

        // List all objects with selection + delete
        for (size_t i = 0; i < scene.objects.size(); i++) {
            ImGui::PushID(static_cast<int>(i));

            // Selectable object row
            bool isSelected = (selectedObjectIndex == static_cast<int>(i));

            // Color coding for selected object
            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.5f, 0.8f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.4f, 0.6f, 0.9f, 0.9f));
            }

            if (ImGui::Selectable(scene.objects[i].name.c_str(), isSelected)) {
                selectedObjectIndex = static_cast<int>(i);
            }

            if (isSelected) {
                ImGui::PopStyleColor(2);
            }

            // Right-click context menu
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Delete")) {
                    if (renderEngine) {
                        renderEngine->deleteObject(i);
                    }
                }
                if (ImGui::MenuItem("Rename")) {
                    // TODO: Implement rename dialog
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        ImGui::Separator();

        // Selected object details
        if (selectedObjectIndex >= 0 && selectedObjectIndex < static_cast<int>(scene.objects.size())) {
            ImGui::Text("Selected: %s", scene.objects[selectedObjectIndex].name.c_str());
            ImGui::Text("Vertices: %zu", scene.objects[selectedObjectIndex].mesh.getVertices().size());
            ImGui::Text("Indices: %zu", scene.objects[selectedObjectIndex].mesh.getIndices().size());

            // Gizmo mode indicator
            ImGui::Separator();
            ImGui::Text("Gizmo Mode:");
            const char* modeNames[] = { "Translate (W)", "Rotate (E)", "Scale (R)" };
            int currentMode = static_cast<int>(gizmoState.mode);
            if (ImGui::Combo("##GizmoMode", &currentMode, modeNames, 3)) {
                gizmoState.mode = static_cast<GizmoMode>(currentMode);
            }
            ImGui::TextWrapped("Press W/E/R to switch modes");

            // Transform controls
            ImGui::Separator();
            auto& transform = scene.objects[selectedObjectIndex].transform;
            ImGui::DragFloat3("Position##Object", &transform.position.x, 0.1f);
            ImGui::DragFloat3("Rotation##Object", &transform.rotation.x, 1.0f);
            ImGui::DragFloat3("Scale##Object", &transform.scale.x, 0.01f);
        }
    } else {
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ======= SECTION 3: Lights =======
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PopStyleColor();

        ImGui::Text("Active Lights: %zu / 8", scene.lights.size());
        ImGui::Separator();

        // Add Light button
        if (ImGui::Button(ICON_FA_CIRCLE_PLUS " Add Light", ImVec2(-1, 0))) {
            if (scene.lights.size() < 8) {
                Light newLight;
                newLight.name = "Light_" + std::to_string(scene.lights.size());
                newLight.type = LightType::POINT;
                newLight.position = glm::vec3(0.0f, 10.0f, 0.0f);
                newLight.color = glm::vec3(1.0f, 1.0f, 1.0f);
                newLight.intensity = 50.0f;
                newLight.range = 100.0f;
                newLight.enabled = true;
                scene.lights.push_back(newLight);
            }
        }

        ImGui::Spacing();

        // List all lights
        for (size_t i = 0; i < scene.lights.size(); i++) {
            ImGui::PushID(static_cast<int>(i));

            Light& light = scene.lights[i];

            // Light header with enable checkbox
            ImGui::Checkbox("##LightEnabled", &light.enabled);
            ImGui::SameLine();

            // Collapsing header for this light
            std::string headerLabel = light.name + " (" +
                (light.type == LightType::POINT ? "Point" :
                 light.type == LightType::DIRECTIONAL ? "Directional" : "Spot") + ")";

            if (ImGui::CollapsingHeader(headerLabel.c_str())) {
                ImGui::Indent();

                // Light name
                char nameBuffer[64];
                strncpy(nameBuffer, light.name.c_str(), sizeof(nameBuffer) - 1);
                nameBuffer[sizeof(nameBuffer) - 1] = '\0';
                if (ImGui::InputText("Name##Light", nameBuffer, sizeof(nameBuffer))) {
                    light.name = std::string(nameBuffer);
                }

                // Light type
                const char* lightTypes[] = { "Point", "Directional", "Spot" };
                int currentType = static_cast<int>(light.type);
                if (ImGui::Combo("Type##Light", &currentType, lightTypes, 3)) {
                    light.type = static_cast<LightType>(currentType);
                }

                ImGui::Separator();

                // Position
                if (light.type == LightType::POINT || light.type == LightType::SPOT) {
                    // Point/Spot lights: Position is the actual light location
                    ImGui::DragFloat3("Position##Light", &light.position.x, 0.5f);
                } else if (light.type == LightType::DIRECTIONAL) {
                    // Directional lights: Position defines where the light "comes from"
                    // Direction is auto-calculated as: sceneCenter - position
                    glm::vec3 sceneCenter(0.0f, 0.0f, 0.0f);
                    if (ImGui::DragFloat3("Light Source Position##Light", &light.position.x, 0.5f)) {
                        // Auto-calculate direction: from position toward scene center
                        light.direction = glm::normalize(sceneCenter - light.position);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Position where the light comes from (direction is auto-calculated toward scene center)");
                    }
                }

                // Direction (manual override for Directional/Spot lights)
                if (light.type == LightType::DIRECTIONAL || light.type == LightType::SPOT) {
                    // Show computed direction (read-only display)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                    ImGui::Text("Direction (computed): (%.3f, %.3f, %.3f)",
                               light.direction.x, light.direction.y, light.direction.z);
                    ImGui::PopStyleColor();

                    // Optional: Manual direction override (collapsed by default)
                    if (ImGui::TreeNode("Manual Direction Override")) {
                        if (ImGui::DragFloat3("Direction##LightManual", &light.direction.x, 0.01f)) {
                            // Auto-normalize direction after manual editing
                            light.direction = glm::normalize(light.direction);
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Manually override direction (usually not needed - position is easier)");
                        }
                        ImGui::TreePop();
                    }

                    // For directional lights, show computed shadow frustum position (read-only)
                    if (light.type == LightType::DIRECTIONAL && light.castsShadows) {
                        glm::vec3 shadowPos = -glm::normalize(light.direction) * light.shadowFarPlane * 0.5f;
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                        ImGui::Text("Shadow Frustum Position: (%.1f, %.1f, %.1f)",
                                   shadowPos.x, shadowPos.y, shadowPos.z);
                        ImGui::PopStyleColor();
                    }
                }

                ImGui::Separator();

                // Color
                ImGui::ColorEdit3("Color##Light", &light.color.x);

                // Intensity
                ImGui::SliderFloat("Intensity##Light", &light.intensity, 0.0f, 500.0f, "%.1f");

                ImGui::Separator();

                // Attenuation (for Point/Spot lights)
                if (light.type == LightType::POINT || light.type == LightType::SPOT) {
                    ImGui::Text("Attenuation:");
                    ImGui::SliderFloat("Range##Light", &light.range, 1.0f, 500.0f, "%.1f");
                    ImGui::SliderFloat("Constant##Light", &light.attenuationConstant, 0.0f, 2.0f, "%.3f");
                    ImGui::SliderFloat("Linear##Light", &light.attenuationLinear, 0.0f, 1.0f, "%.4f");
                    ImGui::SliderFloat("Quadratic##Light", &light.attenuationQuadratic, 0.0f, 0.5f, "%.4f");
                }

                ImGui::Separator();

                // Shadow Properties
                if (ImGui::TreeNode("Shadow Settings")) {
                    ImGui::Checkbox("Cast Shadows##Light", &light.castsShadows);

                    if (light.castsShadows) {
                        ImGui::DragFloat("Shadow Bias##Light", &light.shadowBias, 0.0001f, 0.0f, 0.1f, "%.4f");
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Depth bias to prevent shadow acne");
                        }

                        // Shadow map resolution
                        int resolutions[] = { 512, 1024, 2048, 4096 };
                        const char* resolutionLabels[] = { "512x512", "1024x1024", "2048x2048", "4096x4096" };
                        int currentResIdx = 1;  // Default 1024
                        for (int j = 0; j < 4; j++) {
                            if (light.shadowMapResolution == resolutions[j]) {
                                currentResIdx = j;
                                break;
                            }
                        }
                        if (ImGui::Combo("Shadow Map Resolution##Light", &currentResIdx, resolutionLabels, 4)) {
                            light.shadowMapResolution = resolutions[currentResIdx];
                        }

                        // Light-type-specific shadow settings
                        if (light.type == LightType::DIRECTIONAL) {
                            ImGui::DragFloat("Ortho Size##Light", &light.shadowOrthoSize, 1.0f, 1.0f, 5000.0f, "%.1f");
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("Size of shadow frustum coverage");
                            }
                            ImGui::DragFloat("Near Plane##Light", &light.shadowNearPlane, 0.1f, 0.01f, 100.0f, "%.2f");
                            ImGui::DragFloat("Far Plane##Light", &light.shadowFarPlane, 10.0f, 10.0f, 50000.0f, "%.1f");
                        } else if (light.type == LightType::POINT) {
                            ImGui::DragFloat("Near Plane##Light", &light.shadowNearPlane, 0.01f, 0.01f, 10.0f, "%.3f");
                            ImGui::DragFloat("Far Plane##Light", &light.shadowFarPlane, 1.0f, 1.0f, 500.0f, "%.1f");
                        }

                        // Shadow Map Preview
                        if (renderEngine && renderEngine->getShadowPass()) {
                            ImGui::Separator();

                            if (light.type == LightType::DIRECTIONAL) {
                                // 2D shadow map preview
                                ImGui::Text("Shadow Map Preview:");

                                if (shadowMapImGuiDescriptor == VK_NULL_HANDLE) {
                                    shadowMapImGuiDescriptor = ImGui_ImplVulkan_AddTexture(
                                        renderEngine->getShadowPass()->getShadowMapSampler(),
                                        renderEngine->getShadowPass()->getShadowMapPreviewView(),
                                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                    );
                                }

                                if (shadowMapImGuiDescriptor != VK_NULL_HANDLE) {
                                    float previewSize = ImGui::GetContentRegionAvail().x;
                                    ImGui::Image(shadowMapImGuiDescriptor, ImVec2(previewSize, previewSize));
                                }
                            } else if (light.type == LightType::POINT) {
                                // Determine cube map slot for this light
                                int cubeSlot = -1;
                                {
                                    int slot = 0;
                                    for (size_t li = 0; li < scene.lights.size() && slot < MAX_POINT_SHADOWS; li++) {
                                        const Light& l = scene.lights[li];
                                        if (!l.enabled || !l.castsShadows || l.type != LightType::POINT) continue;
                                        if (li == i) { cubeSlot = slot; break; }
                                        slot++;
                                    }
                                }

                                if (cubeSlot >= 0) {
                                    // Cube map 6-face preview (3x2 grid)
                                    ImGui::Text("Cube Shadow Map Preview (Slot %d):", cubeSlot);

                                    // Create ImGui descriptors for cube faces (lazy init)
                                    if (cubeFaceImGuiDescriptors[cubeSlot][0] == VK_NULL_HANDLE) {
                                        for (int face = 0; face < 6; face++) {
                                            cubeFaceImGuiDescriptors[cubeSlot][face] = ImGui_ImplVulkan_AddTexture(
                                                renderEngine->getShadowPass()->getCubeShadowMapSampler(),
                                                renderEngine->getShadowPass()->getCubeFacePreviewView(cubeSlot, face),
                                                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                            );
                                        }
                                    }

                                    const char* faceNames[] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };
                                    float availWidth = ImGui::GetContentRegionAvail().x;
                                    float faceSize = (availWidth - ImGui::GetStyle().ItemSpacing.x * 2) / 3.0f;

                                    for (int face = 0; face < 6; face++) {
                                        if (face % 3 != 0) ImGui::SameLine();
                                        ImGui::BeginGroup();
                                        ImGui::Text("%s", faceNames[face]);
                                        if (cubeFaceImGuiDescriptors[cubeSlot][face] != VK_NULL_HANDLE) {
                                            ImGui::Image(cubeFaceImGuiDescriptors[cubeSlot][face], ImVec2(faceSize, faceSize));
                                        }
                                        ImGui::EndGroup();
                                    }
                                } else {
                                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                                        "Max point shadows reached (%d/%d)", MAX_POINT_SHADOWS, MAX_POINT_SHADOWS);
                                }
                            }
                        }
                    }

                    ImGui::TreePop();
                }

                ImGui::Separator();

                // Delete button
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                if (ImGui::Button("Delete Light", ImVec2(-1, 0))) {
                    scene.lights.erase(scene.lights.begin() + i);
                    ImGui::PopStyleColor(2);
                    ImGui::Unindent();
                    ImGui::PopID();
                    break;  // Exit loop after deletion to avoid iterator invalidation
                }
                ImGui::PopStyleColor(2);

                ImGui::Unindent();
            }

            ImGui::PopID();
            ImGui::Spacing();
        }

        if (scene.lights.empty()) {
            ImGui::TextWrapped("No lights in scene. Add one to see objects!");
        }
    } else {
        ImGui::PopStyleColor();
    }

    ImGui::End();
}

void ImGuiOverlay::buildContentBrowser(RenderEngine* renderEngine) {
    // Initialize content browser on first call
    if (!contentBrowserInitialized) {
        // Start at repository root
        rootPath = std::filesystem::current_path();
        currentPath = rootPath;

        refreshCurrentDirectory();
        contentBrowserInitialized = true;
        std::cout << "Content Browser initialized at repository root: " << currentPath << std::endl;
    }

    // Get window dimensions
    int windowWidth, windowHeight;
    glfwGetWindowSize(windowHandle, &windowWidth, &windowHeight);

    const float browserHeight = 320.0f;  // Increased height
    const float padding = 10.0f;
    const float rightPanelWidth = 350.0f; // Match the right panel width

    // Position: Bottom of window, full width minus right panel (only on first use)
    ImGui::SetNextWindowPos(ImVec2(padding, windowHeight - browserHeight - padding), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(windowWidth - rightPanelWidth - 3.0f * padding, browserHeight), ImGuiCond_FirstUseEver);

    // Create window (freely movable, resizable, collapsible)
    ImGui::Begin("Content Browser", nullptr);

    // Navigation Bar
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
    ImGui::TextWrapped("%s", currentPath.string().c_str());
    ImGui::PopStyleColor();

    // Navigation buttons
    if (ImGui::Button(ICON_FA_ARROW_LEFT " Back")) {
        if (currentPath.has_parent_path()) {
            navigateToDirectory(currentPath.parent_path());
        }
    }

    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_HOUSE " Root")) {
        navigateToDirectory(rootPath);
    }

    ImGui::SameLine();

    // Create Folder button
    static char newFolderName[128] = "NewFolder";
    ImGui::PushItemWidth(150.0f);
    ImGui::InputText("##NewFolderName", newFolderName, IM_ARRAYSIZE(newFolderName));
    ImGui::PopItemWidth();

    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_CIRCLE_PLUS " Create Folder")) {
        try {
            std::filesystem::path newFolderPath = currentPath / newFolderName;
            if (!std::filesystem::exists(newFolderPath)) {
                std::filesystem::create_directory(newFolderPath);
                std::cout << "Created folder: " << newFolderPath << std::endl;
                refreshCurrentDirectory();
            } else {
                std::cerr << "Folder already exists: " << newFolderPath << std::endl;
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Error creating folder: " << e.what() << std::endl;
        }
    }

    ImGui::Separator();

    // File/Directory Grid (larger, more spacious)
    const float thumbnailSize = 100.0f;  // Increased from 85px
    const float cellPadding = 12.0f;      // More padding between cells
    const float cellSize = thumbnailSize + cellPadding;
    const int columnsCount = std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x) / cellSize));

    ImGui::Columns(columnsCount, nullptr, false);

    // Display directories first
    for (size_t i = 0; i < currentDirectories.size(); i++) {
        const auto& dirEntry = currentDirectories[i];
        ImGui::PushID(static_cast<int>(i));  // Use index as ID (safe!)

        std::string dirName = dirEntry.path().filename().string();

        // Directory button with folder icon
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.35f, 0.45f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.45f, 0.55f, 1.0f));

        // Truncate long names
        std::string displayName = dirName;
        if (displayName.length() > 10) {
            displayName = displayName.substr(0, 7) + "...";
        }

        // Folder icon + name
        std::string buttonLabel = std::string(ICON_FA_FOLDER) + "\n" + displayName;
        if (ImGui::Button(buttonLabel.c_str(), ImVec2(thumbnailSize, thumbnailSize))) {
            navigateToDirectory(dirEntry.path());
        }

        ImGui::PopStyleColor(2);

        // Right-click context menu for directories
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete Folder")) {
                try {
                    std::filesystem::remove_all(dirEntry.path());
                    std::cout << "Deleted folder: " << dirEntry.path() << std::endl;
                    refreshCurrentDirectory();
                } catch (const std::filesystem::filesystem_error& e) {
                    std::cerr << "Error deleting folder: " << e.what() << std::endl;
                }
            }
            ImGui::EndPopup();
        }

        ImGui::NextColumn();
        ImGui::PopID();
    }

    // Display files
    for (size_t i = 0; i < currentFiles.size(); i++) {
        const auto& fileEntry = currentFiles[i];
        ImGui::PushID(static_cast<int>(i + 10000));  // Offset to avoid collision with directories

        std::string filename = fileEntry.path().filename().string();
        std::string extension = fileEntry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        // Determine icon based on file type
        const char* icon = ICON_FA_FILE;
        ImVec4 buttonColor = ImVec4(0.20f, 0.25f, 0.30f, 1.0f);

        if (extension == ".obj" || extension == ".fbx" || extension == ".gltf" || extension == ".glb") {
            icon = ICON_FA_CUBE;
            buttonColor = ImVec4(0.30f, 0.50f, 0.30f, 1.0f); // Green for meshes
        } else if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp") {
            icon = ICON_FA_IMAGE;
            buttonColor = ImVec4(0.50f, 0.30f, 0.50f, 1.0f); // Purple for images
        } else if (extension == ".txt" || extension == ".md") {
            icon = ICON_FA_FILE;
        } else if (extension == ".cpp" || extension == ".h" || extension == ".c" || extension == ".hpp") {
            icon = ICON_FA_FILE_CODE;
            buttonColor = ImVec4(0.40f, 0.40f, 0.50f, 1.0f); // Blue-ish for code
        } else if (extension == ".vert" || extension == ".frag" || extension == ".glsl") {
            icon = ICON_FA_PALETTE;
            buttonColor = ImVec4(0.60f, 0.40f, 0.20f, 1.0f); // Orange for shaders
        }

        // Truncate long filenames
        std::string displayName = filename;
        if (displayName.length() > 10) {
            displayName = displayName.substr(0, 7) + "...";
        }

        ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(buttonColor.x + 0.1f, buttonColor.y + 0.1f, buttonColor.z + 0.1f, 1.0f));

        // File button with icon + name
        std::string buttonLabel = std::string(icon) + "\n" + displayName;
        bool clicked = ImGui::Button(buttonLabel.c_str(), ImVec2(thumbnailSize, thumbnailSize));

        if (clicked && (extension == ".obj") && renderEngine) {
            // Load mesh on click
            std::cout << "Loading mesh: " << fileEntry.path().string() << std::endl;
            renderEngine->loadMesh(fileEntry.path().string());
        }

        ImGui::PopStyleColor(2);

        // Drag & Drop Source (only for meshes)
        if (extension == ".obj" || extension == ".fbx" || extension == ".gltf" || extension == ".glb") {
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                std::string pathStr = fileEntry.path().string();
                ImGui::SetDragDropPayload("CONTENT_BROWSER_MESH", pathStr.c_str(), pathStr.size() + 1);
                ImGui::Text("Loading: %s", filename.c_str());
                ImGui::EndDragDropSource();
            }
        }

        // Right-click context menu for files
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete File")) {
                try {
                    std::filesystem::remove(fileEntry.path());
                    std::cout << "Deleted file: " << fileEntry.path() << std::endl;
                    refreshCurrentDirectory();
                } catch (const std::filesystem::filesystem_error& e) {
                    std::cerr << "Error deleting file: " << e.what() << std::endl;
                }
            }
            ImGui::EndPopup();
        }

        ImGui::NextColumn();
        ImGui::PopID();
    }

    ImGui::Columns(1);

    ImGui::Separator();
    ImGui::Text("%zu folders | %zu files", currentDirectories.size(), currentFiles.size());

    ImGui::End();
}

void ImGuiOverlay::refreshCurrentDirectory() {
    currentDirectories.clear();
    currentFiles.clear();

    if (!std::filesystem::exists(currentPath)) {
        std::cerr << "Content Browser: Path does not exist: " << currentPath << std::endl;
        return;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
            if (entry.is_directory()) {
                currentDirectories.push_back(entry);
            } else if (entry.is_regular_file()) {
                currentFiles.push_back(entry);
            }
        }

        // Sort alphabetically
        std::sort(currentDirectories.begin(), currentDirectories.end(),
                  [](const auto& a, const auto& b) {
                      return a.path().filename().string() < b.path().filename().string();
                  });

        std::sort(currentFiles.begin(), currentFiles.end(),
                  [](const auto& a, const auto& b) {
                      return a.path().filename().string() < b.path().filename().string();
                  });

    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Content Browser: Error reading directory: " << e.what() << std::endl;
    }
}

void ImGuiOverlay::navigateToDirectory(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) {
        std::cerr << "Content Browser: Invalid directory: " << path << std::endl;
        return;
    }

    currentPath = path;
    refreshCurrentDirectory();
    std::cout << "Content Browser: Navigated to " << currentPath << std::endl;
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
    if (shadowMapImGuiDescriptor != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(shadowMapImGuiDescriptor);
        shadowMapImGuiDescriptor = VK_NULL_HANDLE;
    }

    for (int ci = 0; ci < MAX_POINT_SHADOWS; ci++) {
        for (int face = 0; face < 6; face++) {
            if (cubeFaceImGuiDescriptors[ci][face] != VK_NULL_HANDLE) {
                ImGui_ImplVulkan_RemoveTexture(cubeFaceImGuiDescriptors[ci][face]);
                cubeFaceImGuiDescriptors[ci][face] = VK_NULL_HANDLE;
            }
        }
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (imguiPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, imguiPool, nullptr);
        imguiPool = VK_NULL_HANDLE;
    }

    std::cout << "ImGuiOverlay: Cleaned up." << std::endl;
}
