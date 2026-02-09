#include "VulkanCommand.h"
#include "VulkanContext.h"
#include <stdexcept>
#include <iostream>

// ============================================================================
// VULKAN COMMAND IMPLEMENTATION
// ============================================================================

void VulkanCommand::init(VulkanContext* ctx) {
    context = ctx;

    std::cout << "Initializing VulkanCommand..." << std::endl;

    createCommandPool();

    std::cout << "VulkanCommand initialized successfully!" << std::endl;
}

void VulkanCommand::cleanup() {
    std::cout << "Cleaning up VulkanCommand..." << std::endl;

    // Destroy command pool (automatically frees all command buffers allocated from it)
    if (commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(context->getDevice(), commandPool, nullptr);
        commandPool = VK_NULL_HANDLE;
    }

    std::cout << "VulkanCommand cleaned up." << std::endl;
}

void VulkanCommand::createCommandPool() {
    uint32_t graphicsFamily = context->getGraphicsQueueFamily();

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Allow command buffers to be reset

    if (vkCreateCommandPool(context->getDevice(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool!");
    }

    std::cout << "  - Command pool created" << std::endl;
}

void VulkanCommand::allocateCommandBuffers(std::vector<VkCommandBuffer>& outBuffers, uint32_t count) {
    outBuffers.resize(count);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = count;

    if (vkAllocateCommandBuffers(context->getDevice(), &allocInfo, outBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
}

VkCommandBuffer VulkanCommand::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(context->getDevice(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanCommand::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(context->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(context->getGraphicsQueue());

    vkFreeCommandBuffers(context->getDevice(), commandPool, 1, &commandBuffer);
}
