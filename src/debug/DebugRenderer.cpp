#include "DebugRenderer.h"
#include "../core/ShaderManager.h"
#include "../core/PipelineBuilder.h"
#include "../core/VulkanResource.h"
#include "../core/VulkanCommand.h"
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

    uint32_t indexOffset = 0;

    // ========== AXES (LONG - extend far into distance) ==========
    const float axisLength = 1000.0f;
    const float axisThickness = 0.05f;

    // X-Axis (Red) - from -1000 to +1000
    verts.push_back({{-axisLength, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
    verts.push_back({{ axisLength, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}});
    verts.push_back({{ axisLength, axisThickness, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}});
    verts.push_back({{-axisLength, axisThickness, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}});
    inds.insert(inds.end(), {static_cast<uint32_t>(indexOffset+0), static_cast<uint32_t>(indexOffset+1), static_cast<uint32_t>(indexOffset+2), static_cast<uint32_t>(indexOffset+2), static_cast<uint32_t>(indexOffset+3), static_cast<uint32_t>(indexOffset+0)});
    indexOffset += 4;

    // Y-Axis (Green) - from 0 to +1000 (only positive, ground is at y=0)
    verts.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
    verts.push_back({{axisThickness, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}});
    verts.push_back({{axisThickness, axisLength, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}});
    verts.push_back({{0.0f, axisLength, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}});
    inds.insert(inds.end(), {static_cast<uint32_t>(indexOffset+0), static_cast<uint32_t>(indexOffset+1), static_cast<uint32_t>(indexOffset+2), static_cast<uint32_t>(indexOffset+2), static_cast<uint32_t>(indexOffset+3), static_cast<uint32_t>(indexOffset+0)});
    indexOffset += 4;

    // Z-Axis (Blue) - from -1000 to +1000
    verts.push_back({{0.0f, 0.0f, -axisLength}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
    verts.push_back({{axisThickness, 0.0f, -axisLength}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}});
    verts.push_back({{axisThickness, 0.0f, axisLength}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}});
    verts.push_back({{0.0f, 0.0f, axisLength}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}});
    inds.insert(inds.end(), {static_cast<uint32_t>(indexOffset+0), static_cast<uint32_t>(indexOffset+1), static_cast<uint32_t>(indexOffset+2), static_cast<uint32_t>(indexOffset+2), static_cast<uint32_t>(indexOffset+3), static_cast<uint32_t>(indexOffset+0)});
    indexOffset += 4;

    // ========== GRID LINES (Gray) - XZ plane at Y=0 ==========
    // Line along X at Z=-1
    verts.push_back({{-1.0f, 0.0f, -1.0f}, {0.3f, 0.3f, 0.3f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
    verts.push_back({{ 1.0f, 0.0f, -1.0f}, {0.3f, 0.3f, 0.3f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}});
    verts.push_back({{ 1.0f, 0.01f, -1.0f}, {0.3f, 0.3f, 0.3f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}});
    verts.push_back({{-1.0f, 0.01f, -1.0f}, {0.3f, 0.3f, 0.3f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}});
    inds.insert(inds.end(), {static_cast<uint32_t>(indexOffset+0), static_cast<uint32_t>(indexOffset+1), static_cast<uint32_t>(indexOffset+2), static_cast<uint32_t>(indexOffset+2), static_cast<uint32_t>(indexOffset+3), static_cast<uint32_t>(indexOffset+0)});
    indexOffset += 4;

    // Line along X at Z=1
    verts.push_back({{-1.0f, 0.0f, 1.0f}, {0.3f, 0.3f, 0.3f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
    verts.push_back({{ 1.0f, 0.0f, 1.0f}, {0.3f, 0.3f, 0.3f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}});
    verts.push_back({{ 1.0f, 0.01f, 1.0f}, {0.3f, 0.3f, 0.3f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}});
    verts.push_back({{-1.0f, 0.01f, 1.0f}, {0.3f, 0.3f, 0.3f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}});
    inds.insert(inds.end(), {static_cast<uint32_t>(indexOffset+0), static_cast<uint32_t>(indexOffset+1), static_cast<uint32_t>(indexOffset+2), static_cast<uint32_t>(indexOffset+2), static_cast<uint32_t>(indexOffset+3), static_cast<uint32_t>(indexOffset+0)});
    indexOffset += 4;

    // Line along Z at X=-1
    verts.push_back({{-1.0f, 0.0f, -1.0f}, {0.3f, 0.3f, 0.3f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
    verts.push_back({{-1.0f, 0.0f, 1.0f}, {0.3f, 0.3f, 0.3f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}});
    verts.push_back({{-0.99f, 0.01f, 1.0f}, {0.3f, 0.3f, 0.3f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}});
    verts.push_back({{-0.99f, 0.01f, -1.0f}, {0.3f, 0.3f, 0.3f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}});
    inds.insert(inds.end(), {static_cast<uint32_t>(indexOffset+0), static_cast<uint32_t>(indexOffset+1), static_cast<uint32_t>(indexOffset+2), static_cast<uint32_t>(indexOffset+2), static_cast<uint32_t>(indexOffset+3), static_cast<uint32_t>(indexOffset+0)});
    indexOffset += 4;

    // Line along Z at X=1
    verts.push_back({{1.0f, 0.0f, -1.0f}, {0.3f, 0.3f, 0.3f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
    verts.push_back({{1.0f, 0.0f, 1.0f}, {0.3f, 0.3f, 0.3f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}});
    verts.push_back({{1.01f, 0.01f, 1.0f}, {0.3f, 0.3f, 0.3f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}});
    verts.push_back({{1.01f, 0.01f, -1.0f}, {0.3f, 0.3f, 0.3f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}});
    inds.insert(inds.end(), {static_cast<uint32_t>(indexOffset+0), static_cast<uint32_t>(indexOffset+1), static_cast<uint32_t>(indexOffset+2), static_cast<uint32_t>(indexOffset+2), static_cast<uint32_t>(indexOffset+3), static_cast<uint32_t>(indexOffset+0)});

    debugMesh.setGeometry(std::move(verts), std::move(inds));
    std::cout << "DebugRenderer: Debug geometry built." << std::endl;
}

void DebugRenderer::createDebugPipeline(VkDevice device, ShaderManager& shaderManager,
                                         VkExtent2D extent, VkDescriptorSetLayout layout,
                                         VkRenderPass renderPass) {
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    auto result = PipelineBuilder(device)
        .setShaders(shaderManager,
                    std::string(SHADER_DIR) + "/debug.vert.spv",
                    std::string(SHADER_DIR) + "/debug.frag.spv")
        .setVertexInput(bindingDescription, attributeDescriptions.data(),
                        static_cast<uint32_t>(attributeDescriptions.size()))
        .setViewport(extent)
        .setCullMode(VK_CULL_MODE_NONE)
        .setDepthTest(true, false, VK_COMPARE_OP_ALWAYS)
        .setDescriptorLayouts({layout})
        .setRenderPass(renderPass)
        .build();

    debugPipeline = result.pipeline;
    debugPipelineLayout = result.layout;
    std::cout << "DebugRenderer: Debug pipeline created." << std::endl;
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
                           bool drawGrid, bool drawAxes) {
    // Draw 1: Infinite Grid (procedural, depth-aware)
    if (drawGrid) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gridPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                gridPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        vkCmdDraw(cmd, 6, 1, 0, 0);
    }

    // Draw 2: Debug Axes (always on top, unlit)
    if (drawAxes) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, debugPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                debugPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        debugMesh.bind(cmd);
        debugMesh.draw(cmd);
    }
}

void DebugRenderer::cleanup(VkDevice device) {
    debugMesh.cleanup(device);

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
