#include "DescriptorManager.h"
#include <stdexcept>
#include <iostream>

DescriptorManager::DescriptorManager(VkDevice device) : device(device) {}

DescriptorManager::~DescriptorManager() {
    cleanup();
}

VkDescriptorSetLayout DescriptorManager::createLayout(
    const std::vector<VkDescriptorSetLayoutBinding>& bindings) {

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkDescriptorSetLayout layout;
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error("DescriptorManager: failed to create descriptor set layout!");
    }

    layouts.push_back(layout);
    return layout;
}

void DescriptorManager::createPool(
    const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets) {

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = maxSets;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error("DescriptorManager: failed to create descriptor pool!");
    }
}

std::vector<VkDescriptorSet> DescriptorManager::allocateSets(
    VkDescriptorSetLayout layout, uint32_t count) {

    std::vector<VkDescriptorSetLayout> setLayouts(count, layout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = count;
    allocInfo.pSetLayouts = setLayouts.data();

    std::vector<VkDescriptorSet> sets(count);
    if (vkAllocateDescriptorSets(device, &allocInfo, sets.data()) != VK_SUCCESS) {
        throw std::runtime_error("DescriptorManager: failed to allocate descriptor sets!");
    }

    return sets;
}

void DescriptorManager::writeBuffer(VkDescriptorSet set, uint32_t binding,
                                     VkBuffer buffer, VkDeviceSize range,
                                     VkDeviceSize offset, VkDescriptorType type) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer;
    bufferInfo.offset = offset;
    bufferInfo.range = range;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = type;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void DescriptorManager::writeImage(VkDescriptorSet set, uint32_t binding,
                                    VkImageView imageView, VkSampler sampler,
                                    VkImageLayout layout, VkDescriptorType type) {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = layout;
    imageInfo.imageView = imageView;
    imageInfo.sampler = sampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = type;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void DescriptorManager::cleanup() {
    if (pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, pool, nullptr);
        pool = VK_NULL_HANDLE;
    }

    for (auto layout : layouts) {
        vkDestroyDescriptorSetLayout(device, layout, nullptr);
    }
    layouts.clear();
}
