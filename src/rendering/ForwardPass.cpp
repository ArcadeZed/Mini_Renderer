#include "ForwardPass.h"
#include "ILightingModel.h"
#include "PhongLighting.h"
#include "../core/ShaderManager.h"
#include "../core/PipelineBuilder.h"
#include "../mesh/Mesh.h"
#include "../scene/Scene.h"
#include <iostream>
#include <string>

void ForwardPass::init(VkDevice device,
                       ShaderManager& shaderManager,
                       VkExtent2D swapchainExtent,
                       VkDescriptorSetLayout descriptorSetLayout,
                       VkRenderPass renderPass) {
    // Store parameters for later pipeline rebuilds
    this->device = device;
    this->shaderManager = &shaderManager;
    this->swapchainExtent = swapchainExtent;
    this->descriptorSetLayout = descriptorSetLayout;
    this->renderPass = renderPass;

    // Default to Phong lighting
    static PhongLighting defaultPhong;
    currentModel = &defaultPhong;

    buildPipeline();
}

void ForwardPass::buildPipeline() {
    // Clean up old pipeline if it exists
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
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

    auto result = PipelineBuilder(device)
        .setShaders(*shaderManager,
                    std::string(SHADER_DIR) + "/shader.vert.spv",
                    currentModel->getFragmentShaderPath())
        .setVertexInput(bindingDescription, attributeDescriptions.data(),
                        static_cast<uint32_t>(attributeDescriptions.size()))
        .setViewport(swapchainExtent)
        .setCullMode(VK_CULL_MODE_BACK_BIT)
        .setDepthTest(true, true, VK_COMPARE_OP_LESS)
        .setDescriptorLayouts({descriptorSetLayout})
        .setRenderPass(renderPass)
        .build();

    pipeline = result.pipeline;
    pipelineLayout = result.layout;
    std::cout << "ForwardPass: Graphics pipeline created with "
              << currentModel->getName() << " lighting." << std::endl;
}

void ForwardPass::record(VkCommandBuffer cmd,
                         VkDescriptorSet globalDescriptorSet,
                         const Scene& scene,
                         const std::vector<VkDescriptorSet>& materialDescriptorSets) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Draw all scene objects
    for (const auto& obj : scene.objects) {
        if (!obj.mesh.isUploaded()) continue;

        obj.mesh.bind(cmd);

        // Multi-material mesh: loop over submeshes and bind per-material descriptor sets
        if (obj.mesh.hasMultipleMaterials()) {
            const auto& submeshes = obj.mesh.getSubMeshes();
            for (size_t i = 0; i < submeshes.size(); i++) {
                const auto& submesh = submeshes[i];

                // Bind descriptor set for this material
                if (submesh.materialIndex < materialDescriptorSets.size()) {
                    VkDescriptorSet matDescSet = materialDescriptorSets[submesh.materialIndex];
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            pipelineLayout, 0, 1, &matDescSet, 0, nullptr);
                }

                // Draw this submesh
                obj.mesh.drawSubmesh(cmd, static_cast<uint32_t>(i));
            }
        } else {
            // Single-material mesh: bind global descriptor set and draw entire mesh
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayout, 0, 1, &globalDescriptorSet, 0, nullptr);
            obj.mesh.draw(cmd);
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
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
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
