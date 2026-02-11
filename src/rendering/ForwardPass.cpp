#include "ForwardPass.h"
#include "ILightingModel.h"
#include "PhongLighting.h"
#include "MaterialManager.h"  // For getAlphaMode() queries
#include "RenderEngine.h"  // For PushConstantObject
#include "../core/ShaderManager.h"
#include "../core/PipelineBuilder.h"
#include "../mesh/Mesh.h"
#include "../scene/Scene.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <iostream>
#include <string>

void ForwardPass::init(VkDevice device,
                       ShaderManager& shaderManager,
                       VkExtent2D swapchainExtent,
                       VkDescriptorSetLayout descriptorSetLayout,
                       VkDescriptorSetLayout pbrDescriptorSetLayout,
                       VkRenderPass renderPass,
                       MaterialManager* materialManager) {
    // Store parameters for later pipeline rebuilds
    this->device = device;
    this->shaderManager = &shaderManager;
    this->materialManager = materialManager;
    this->swapchainExtent = swapchainExtent;
    this->descriptorSetLayout = descriptorSetLayout;
    this->pbrDescriptorSetLayout = pbrDescriptorSetLayout;
    this->renderPass = renderPass;

    // Default to Phong lighting
    static PhongLighting defaultPhong;
    currentModel = &defaultPhong;

    buildPipeline();
}

void ForwardPass::buildPipeline() {
    // Clean up old pipelines if they exist
    if (opaquePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, opaquePipeline, nullptr);
        opaquePipeline = VK_NULL_HANDLE;
    }
    if (blendedPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, blendedPipeline, nullptr);
        blendedPipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }

    if (!currentModel) {
        std::cerr << "ForwardPass: No lighting model set!" << std::endl;
        return;
    }

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    // Push constant range for per-object model matrix + material index
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4) + sizeof(int);  // 64 + 4 = 68 bytes

    // Build OPAQUE pipeline (no blending)
    auto opaqueResult = PipelineBuilder(device)
        .setShaders(*shaderManager,
                    std::string(SHADER_DIR) + "/shader.vert.spv",
                    currentModel->getFragmentShaderPath())
        .setVertexInput(bindingDescription, attributeDescriptions.data(),
                        static_cast<uint32_t>(attributeDescriptions.size()))
        .setViewport(swapchainExtent)
        .setCullMode(VK_CULL_MODE_BACK_BIT)
        .setDepthTest(true, true, VK_COMPARE_OP_LESS)  // Depth write ON
        .setDescriptorLayouts({descriptorSetLayout, pbrDescriptorSetLayout})
        .setPushConstants({pushConstantRange})
        .setRenderPass(renderPass)
        .build();

    opaquePipeline = opaqueResult.pipeline;
    pipelineLayout = opaqueResult.layout;

    // Build BLENDED pipeline (alpha blending enabled)
    auto blendedResult = PipelineBuilder(device)
        .setShaders(*shaderManager,
                    std::string(SHADER_DIR) + "/shader.vert.spv",
                    currentModel->getFragmentShaderPath())
        .setVertexInput(bindingDescription, attributeDescriptions.data(),
                        static_cast<uint32_t>(attributeDescriptions.size()))
        .setViewport(swapchainExtent)
        .setCullMode(VK_CULL_MODE_BACK_BIT)
        .setDepthTest(true, false, VK_COMPARE_OP_LESS)  // Depth write OFF (read only)
        .setBlending(true)  // Enable alpha blending!
        .setDescriptorLayouts({descriptorSetLayout, pbrDescriptorSetLayout})
        .setPushConstants({pushConstantRange})
        .setRenderPass(renderPass)
        .build();

    blendedPipeline = blendedResult.pipeline;
    // Note: blendedResult.layout is identical to pipelineLayout, we could destroy one
    // but for simplicity we keep both (will be cleaned up anyway)

    std::cout << "ForwardPass: Graphics pipelines created (opaque + blended) with "
              << currentModel->getName() << " lighting." << std::endl;
}

void ForwardPass::record(VkCommandBuffer cmd,
                         VkDescriptorSet globalDescriptorSet,
                         VkDescriptorSet pbrDescriptorSet,
                         const Scene& scene) {
    // Bind descriptor sets (shared by both passes)
    VkDescriptorSet sets[2] = {globalDescriptorSet, pbrDescriptorSet};
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, 0, 2, sets, 0, nullptr);

    // ===== PASS 1: OPAQUE & MASKED OBJECTS =====
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, opaquePipeline);

    for (const auto& obj : scene.objects) {
        if (!obj.mesh.isUploaded()) continue;

        // Skip alpha-blended objects (alphaMode == 2)
        if (materialManager && materialManager->getAlphaMode(obj.materialIndex) == 2) {
            continue;  // Render in second pass
        }
        if (!obj.mesh.isUploaded()) continue;

        // Push this object's model matrix + material index as push constants
        PushConstantObject pushConst{};
        pushConst.model = obj.transform.getModelMatrix();
        pushConst.materialIndex = obj.materialIndex;
        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(PushConstantObject), &pushConst);

        obj.mesh.bind(cmd);

        // Multi-material mesh: loop over submeshes and bind per-material descriptor sets from object
        if (obj.mesh.hasMultipleMaterials() && obj.useMultiMaterial) {
            const auto& submeshes = obj.mesh.getSubMeshes();
            for (size_t i = 0; i < submeshes.size(); i++) {
                const auto& submesh = submeshes[i];

                // Bind descriptor set for this material (from this object's materialResources)
                if (submesh.materialIndex < obj.materialResources.size()) {
                    VkDescriptorSet matDescSet = obj.materialResources[submesh.materialIndex].descriptorSet;
                    VkDescriptorSet multiMatSets[2] = {matDescSet, pbrDescriptorSet};
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            pipelineLayout, 0, 2, multiMatSets, 0, nullptr);
                }

                // Draw this submesh
                obj.mesh.drawSubmesh(cmd, static_cast<uint32_t>(i));
            }
        } else {
            // Already bound both sets at the top, just draw
            obj.mesh.draw(cmd);
        }
    }

    // ===== PASS 2: ALPHA-BLENDED OBJECTS =====
    // Render transparent objects with blending enabled (depth write OFF)
    // Sorted back-to-front for correct transparency layering
    if (!materialManager) return;  // Can't determine alphaMode without MaterialManager

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blendedPipeline);

    // Collect all blended objects
    std::vector<const SceneObject*> blendedObjects;
    for (const auto& obj : scene.objects) {
        if (!obj.mesh.isUploaded()) continue;
        if (materialManager->getAlphaMode(obj.materialIndex) == 2) {
            blendedObjects.push_back(&obj);
        }
    }

    // Sort back-to-front (farthest from camera first) for correct alpha blending
    if (!blendedObjects.empty()) {
        glm::vec3 camPos = scene.camera.getPosition();
        std::sort(blendedObjects.begin(), blendedObjects.end(),
            [&camPos](const SceneObject* a, const SceneObject* b) {
                // Use transform position as object center for distance calculation
                float distA = glm::length(a->transform.position - camPos);
                float distB = glm::length(b->transform.position - camPos);
                return distA > distB;  // Farthest first!
            });
    }

    // Render sorted blended objects
    for (const auto* obj : blendedObjects) {
        // Push this object's model matrix + material index as push constants
        PushConstantObject pushConst{};
        pushConst.model = obj->transform.getModelMatrix();
        pushConst.materialIndex = obj->materialIndex;
        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(PushConstantObject), &pushConst);

        obj->mesh.bind(cmd);

        // Multi-material mesh: loop over submeshes
        if (obj->mesh.hasMultipleMaterials() && obj->useMultiMaterial) {
            const auto& submeshes = obj->mesh.getSubMeshes();
            for (size_t i = 0; i < submeshes.size(); i++) {
                const auto& submesh = submeshes[i];

                // Bind descriptor set for this material
                if (submesh.materialIndex < obj->materialResources.size()) {
                    VkDescriptorSet matDescSet = obj->materialResources[submesh.materialIndex].descriptorSet;
                    VkDescriptorSet multiMatSets[2] = {matDescSet, pbrDescriptorSet};
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            pipelineLayout, 0, 2, multiMatSets, 0, nullptr);
                }

                // Draw this submesh
                obj->mesh.drawSubmesh(cmd, static_cast<uint32_t>(i));
            }
        } else {
            // Already bound both sets at the top, just draw
            obj->mesh.draw(cmd);
        }
    }
}

void ForwardPass::setLightingModel(ILightingModel* model) {
    if (model && model != currentModel) {
        currentModel = model;
        std::cout << "ForwardPass: Switching to " << model->getName() << " lighting..." << std::endl;
        buildPipeline();
    }
}

void ForwardPass::cleanup(VkDevice device) {
    if (opaquePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, opaquePipeline, nullptr);
        opaquePipeline = VK_NULL_HANDLE;
    }
    if (blendedPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, blendedPipeline, nullptr);
        blendedPipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }
}

void ForwardPass::updateExtent(VkExtent2D newExtent) {
    swapchainExtent = newExtent;
    buildPipeline();
}
