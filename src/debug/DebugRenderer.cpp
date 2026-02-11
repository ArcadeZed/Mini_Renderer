#include "DebugRenderer.h"
#include "../core/ShaderManager.h"
#include "../core/PipelineBuilder.h"
#include "../core/VulkanResource.h"
#include "../core/VulkanCommand.h"
#include "../scene/Scene.h"
#include <iostream>
#include <string>

void DebugRenderer::init(VkDevice device,
                         ShaderManager& shaderManager,
                         VkExtent2D swapchainExtent,
                         VkDescriptorSetLayout descriptorSetLayout,
                         VkRenderPass renderPass,
                         VulkanResource& resource,
                         VulkanCommand& command) {
    // Cache parameters for later pipeline rebuilds
    this->device = device;
    this->shaderManager = &shaderManager;
    this->swapchainExtent = swapchainExtent;
    this->descriptorSetLayout = descriptorSetLayout;
    this->renderPass = renderPass;

    buildDebugGeometry();
    debugMesh.upload(device, resource, command);

    createDebugPipeline(device, shaderManager, swapchainExtent,
                        descriptorSetLayout, renderPass);
    createGridPipeline(device, shaderManager, swapchainExtent,
                       descriptorSetLayout, renderPass);
}

void DebugRenderer::buildDebugGeometry() {
    std::vector<Vertex> verts;
    std::vector<uint32_t> inds;

    // ========== AXES (SIMPLE LINES) ==========
    // Geometry Shader will create quads with constant screen-space thickness
    const float axisLength = 1000.0f;

    // ========== X-Axis (Red) - from -1000 to +1000 ==========
    verts.push_back({{-axisLength, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
    verts.push_back({{ axisLength, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
    inds.push_back(0);
    inds.push_back(1);

    // ========== Y-Axis (Green) - from 0 to +1000 (only positive) ==========
    verts.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
    verts.push_back({{0.0f, axisLength, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
    inds.push_back(2);
    inds.push_back(3);

    // ========== Z-Axis (Blue) - from -1000 to +1000 ==========
    verts.push_back({{0.0f, 0.0f, -axisLength}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
    verts.push_back({{0.0f, 0.0f,  axisLength}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
    inds.push_back(4);
    inds.push_back(5);

    debugMesh.setGeometry(std::move(verts), std::move(inds));
    std::cout << "DebugRenderer: Debug geometry built (lines for geometry shader)." << std::endl;
}

void DebugRenderer::buildLightGizmoGeometry(const Scene& scene) {
    std::vector<Vertex> lightVerts;
    std::vector<uint32_t> lightInds;
    uint32_t indexOffset = 0;

    for (const auto& light : scene.lights) {
        if (light.type == LightType::DIRECTIONAL) {
            // Directional Light: Draw arrow from light.position pointing in direction
            glm::vec3 sceneCenter(0.0f, 0.0f, 0.0f);
            glm::vec3 startPos = light.position;  // Start at the light source position
            glm::vec3 endPos = sceneCenter;        // Point toward scene center

            // Yellow color for directional lights
            glm::vec3 color(1.0f, 1.0f, 0.0f);

            // Arrow shaft
            lightVerts.push_back({startPos, color, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
            lightVerts.push_back({endPos, color, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
            lightInds.push_back(indexOffset);
            lightInds.push_back(indexOffset + 1);
            indexOffset += 2;

            // Arrow head (simple cone as 3 lines)
            glm::vec3 dir = glm::normalize(light.direction);
            glm::vec3 perpendicular1 = glm::normalize(glm::cross(dir, glm::vec3(0.0f, 1.0f, 0.0f)));
            if (glm::length(perpendicular1) < 0.01f) {
                perpendicular1 = glm::normalize(glm::cross(dir, glm::vec3(1.0f, 0.0f, 0.0f)));
            }
            glm::vec3 perpendicular2 = glm::normalize(glm::cross(dir, perpendicular1));

            float arrowHeadSize = 2.0f;
            glm::vec3 arrowBase = endPos - dir * arrowHeadSize;
            glm::vec3 arrowTip1 = arrowBase + perpendicular1 * arrowHeadSize * 0.5f;
            glm::vec3 arrowTip2 = arrowBase - perpendicular1 * arrowHeadSize * 0.5f;
            glm::vec3 arrowTip3 = arrowBase + perpendicular2 * arrowHeadSize * 0.5f;

            lightVerts.push_back({endPos, color, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
            lightVerts.push_back({arrowTip1, color, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
            lightInds.push_back(indexOffset);
            lightInds.push_back(indexOffset + 1);
            indexOffset += 2;

            lightVerts.push_back({endPos, color, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
            lightVerts.push_back({arrowTip2, color, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
            lightInds.push_back(indexOffset);
            lightInds.push_back(indexOffset + 1);
            indexOffset += 2;

            lightVerts.push_back({endPos, color, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
            lightVerts.push_back({arrowTip3, color, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
            lightInds.push_back(indexOffset);
            lightInds.push_back(indexOffset + 1);
            indexOffset += 2;

        } else if (light.type == LightType::POINT) {
            // Point Light: Draw wireframe sphere at position
            glm::vec3 center = light.position;
            float radius = 1.0f;

            // Orange color for point lights
            glm::vec3 color(1.0f, 0.7f, 0.0f);

            // Simple sphere approximation: 3 orthogonal circles (XY, XZ, YZ planes)
            const int segments = 16;

            // XY plane circle
            for (int i = 0; i < segments; ++i) {
                float angle1 = (float)i / segments * 2.0f * glm::pi<float>();
                float angle2 = (float)(i + 1) / segments * 2.0f * glm::pi<float>();

                glm::vec3 p1 = center + glm::vec3(cos(angle1) * radius, sin(angle1) * radius, 0.0f);
                glm::vec3 p2 = center + glm::vec3(cos(angle2) * radius, sin(angle2) * radius, 0.0f);

                lightVerts.push_back({p1, color, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
                lightVerts.push_back({p2, color, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
                lightInds.push_back(indexOffset);
                lightInds.push_back(indexOffset + 1);
                indexOffset += 2;
            }

            // XZ plane circle
            for (int i = 0; i < segments; ++i) {
                float angle1 = (float)i / segments * 2.0f * glm::pi<float>();
                float angle2 = (float)(i + 1) / segments * 2.0f * glm::pi<float>();

                glm::vec3 p1 = center + glm::vec3(cos(angle1) * radius, 0.0f, sin(angle1) * radius);
                glm::vec3 p2 = center + glm::vec3(cos(angle2) * radius, 0.0f, sin(angle2) * radius);

                lightVerts.push_back({p1, color, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
                lightVerts.push_back({p2, color, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
                lightInds.push_back(indexOffset);
                lightInds.push_back(indexOffset + 1);
                indexOffset += 2;
            }

            // YZ plane circle
            for (int i = 0; i < segments; ++i) {
                float angle1 = (float)i / segments * 2.0f * glm::pi<float>();
                float angle2 = (float)(i + 1) / segments * 2.0f * glm::pi<float>();

                glm::vec3 p1 = center + glm::vec3(0.0f, cos(angle1) * radius, sin(angle1) * radius);
                glm::vec3 p2 = center + glm::vec3(0.0f, cos(angle2) * radius, sin(angle2) * radius);

                lightVerts.push_back({p1, color, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
                lightVerts.push_back({p2, color, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
                lightInds.push_back(indexOffset);
                lightInds.push_back(indexOffset + 1);
                indexOffset += 2;
            }
        }
    }

    lightGizmoMesh.setGeometry(std::move(lightVerts), std::move(lightInds));
}

void DebugRenderer::updateLightGizmos(const Scene& scene, VkDevice device,
                                      VulkanResource& resource, VulkanCommand& command) {
    // Cleanup old gizmo mesh
    lightGizmoMesh.cleanup(device);

    // Rebuild geometry from current scene lights
    buildLightGizmoGeometry(scene);

    // Upload to GPU
    if (lightGizmoMesh.getIndexCount() > 0) {
        lightGizmoMesh.upload(device, resource, command);
    }
}

void DebugRenderer::createDebugPipeline(VkDevice device, ShaderManager& shaderManager,
                                         VkExtent2D extent, VkDescriptorSetLayout layout,
                                         VkRenderPass renderPass) {
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    // Push constants for viewport size and line width
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(float) * 3;  // vec2 viewportSize + float lineWidth

    auto result = PipelineBuilder(device)
        .setShaders(shaderManager,
                    std::string(SHADER_DIR) + "/debug.vert.spv",
                    std::string(SHADER_DIR) + "/debug.geom.spv",
                    std::string(SHADER_DIR) + "/debug.frag.spv")
        .setVertexInput(bindingDescription, attributeDescriptions.data(),
                        static_cast<uint32_t>(attributeDescriptions.size()))
        .setTopology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
        .setViewport(extent)
        .setCullMode(VK_CULL_MODE_NONE)
        .setDepthTest(true, false, VK_COMPARE_OP_ALWAYS)
        .setDescriptorLayouts({layout})
        .setPushConstants({pushConstantRange})
        .setRenderPass(renderPass)
        .build();

    debugPipeline = result.pipeline;
    debugPipelineLayout = result.layout;
    std::cout << "DebugRenderer: Debug pipeline created (with geometry shader)." << std::endl;
}

void DebugRenderer::createGridPipeline(VkDevice device, ShaderManager& shaderManager,
                                        VkExtent2D extent, VkDescriptorSetLayout layout,
                                        VkRenderPass renderPass) {
    auto result = PipelineBuilder(device)
        .setShaders(shaderManager,
                    std::string(SHADER_DIR) + "/infinite_grid.vert.spv",
                    std::string(SHADER_DIR) + "/infinite_grid.frag.spv")
        .setViewport(extent)
        .setCullMode(VK_CULL_MODE_NONE)
        .setDepthTest(true, true, VK_COMPARE_OP_LESS)
        .setBlending(true)
        .setDescriptorLayouts({layout})
        .setRenderPass(renderPass)
        .build();

    gridPipeline = result.pipeline;
    gridPipelineLayout = result.layout;
    std::cout << "DebugRenderer: Grid pipeline created." << std::endl;
}

void DebugRenderer::record(VkCommandBuffer cmd, VkDescriptorSet descriptorSet,
                           const class Scene& scene,
                           bool drawGrid, bool drawAxes, bool drawLightGizmos) {
    // Draw 1: Infinite Grid (procedural, depth-aware)
    if (drawGrid) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gridPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                gridPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        vkCmdDraw(cmd, 6, 1, 0, 0);
    }

    // Draw 2: Debug Axes (always on top, constant screen-space thickness)
    if (drawAxes) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, debugPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                debugPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        // Push constants: viewport size + line width
        struct {
            float viewportWidth;
            float viewportHeight;
            float lineWidth;
        } pushConstants;
        pushConstants.viewportWidth = static_cast<float>(swapchainExtent.width);
        pushConstants.viewportHeight = static_cast<float>(swapchainExtent.height);
        pushConstants.lineWidth = 3.0f;  // Line width in pixels

        vkCmdPushConstants(cmd, debugPipelineLayout, VK_SHADER_STAGE_GEOMETRY_BIT,
                           0, sizeof(pushConstants), &pushConstants);

        debugMesh.bind(cmd);
        debugMesh.draw(cmd);
    }

    // Draw 3: Light Gizmos (visual indicators for light positions/directions)
    if (drawLightGizmos && lightGizmoMesh.getIndexCount() > 0) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, debugPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                debugPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        // Push constants: viewport size + line width
        struct {
            float viewportWidth;
            float viewportHeight;
            float lineWidth;
        } pushConstants;
        pushConstants.viewportWidth = static_cast<float>(swapchainExtent.width);
        pushConstants.viewportHeight = static_cast<float>(swapchainExtent.height);
        pushConstants.lineWidth = 2.0f;  // Slightly thinner than axes

        vkCmdPushConstants(cmd, debugPipelineLayout, VK_SHADER_STAGE_GEOMETRY_BIT,
                           0, sizeof(pushConstants), &pushConstants);

        lightGizmoMesh.bind(cmd);
        lightGizmoMesh.draw(cmd);
    }
}

void DebugRenderer::cleanup(VkDevice device) {
    debugMesh.cleanup(device);
    lightGizmoMesh.cleanup(device);

    if (debugPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, debugPipeline, nullptr);
        debugPipeline = VK_NULL_HANDLE;
    }
    if (debugPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, debugPipelineLayout, nullptr);
        debugPipelineLayout = VK_NULL_HANDLE;
    }
    if (gridPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, gridPipeline, nullptr);
        gridPipeline = VK_NULL_HANDLE;
    }
    if (gridPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, gridPipelineLayout, nullptr);
        gridPipelineLayout = VK_NULL_HANDLE;
    }
}

void DebugRenderer::updateExtent(VkExtent2D newExtent) {
    swapchainExtent = newExtent;

    // Rebuild both pipelines with new extent
    if (debugPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, debugPipeline, nullptr);
        vkDestroyPipelineLayout(device, debugPipelineLayout, nullptr);
        debugPipeline = VK_NULL_HANDLE;
        debugPipelineLayout = VK_NULL_HANDLE;
    }
    if (gridPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, gridPipeline, nullptr);
        vkDestroyPipelineLayout(device, gridPipelineLayout, nullptr);
        gridPipeline = VK_NULL_HANDLE;
        gridPipelineLayout = VK_NULL_HANDLE;
    }

    createDebugPipeline(device, *shaderManager, swapchainExtent,
                        descriptorSetLayout, renderPass);
    createGridPipeline(device, *shaderManager, swapchainExtent,
                       descriptorSetLayout, renderPass);
}
