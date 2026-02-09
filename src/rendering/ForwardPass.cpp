#include "ForwardPass.h"
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
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    auto result = PipelineBuilder(device)
        .setShaders(shaderManager,
                    std::string(SHADER_DIR) + "/shader.vert.spv",
                    std::string(SHADER_DIR) + "/shader.frag.spv")
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
    std::cout << "ForwardPass: Graphics pipeline created." << std::endl;
}

void ForwardPass::record(VkCommandBuffer cmd,
                         VkDescriptorSet descriptorSet,
                         const Scene& scene) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    for (const auto& obj : scene.objects) {
        if (obj.mesh.isUploaded()) {
            obj.mesh.bind(cmd);
            obj.mesh.draw(cmd);
        }
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
