#pragma once

#include <vulkan/vulkan.h>
#include <vector>

// Manages Vulkan descriptor set layouts, pools, and sets.
// Centralizes descriptor boilerplate and tracks resources for cleanup.
// Usage:
//   auto layout = descriptorManager.createLayout({uboBinding, samplerBinding});
//   descriptorManager.createPool(poolSizes, maxSets);
//   auto sets = descriptorManager.allocateSets(layout, count);
//   descriptorManager.writeBuffer(sets[i], 0, buffer, sizeof(UBO));
//   descriptorManager.writeImage(sets[i], 1, imageView, sampler);
class DescriptorManager {
public:
    explicit DescriptorManager(VkDevice device);
    ~DescriptorManager();

    // Create a descriptor set layout from bindings. Owned by this manager.
    VkDescriptorSetLayout createLayout(
        const std::vector<VkDescriptorSetLayoutBinding>& bindings);

    // Create a descriptor pool. Only one pool at a time.
    void createPool(
        const std::vector<VkDescriptorPoolSize>& poolSizes,
        uint32_t maxSets);

    // Allocate descriptor sets from the pool, all using the same layout.
    std::vector<VkDescriptorSet> allocateSets(
        VkDescriptorSetLayout layout, uint32_t count);

    // Write a buffer descriptor (immediately updates the set).
    void writeBuffer(VkDescriptorSet set, uint32_t binding,
                     VkBuffer buffer, VkDeviceSize range,
                     VkDeviceSize offset = 0,
                     VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    // Write an image descriptor (immediately updates the set).
    void writeImage(VkDescriptorSet set, uint32_t binding,
                    VkImageView imageView, VkSampler sampler,
                    VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VkDescriptorType type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    // Write an array of image descriptors (for texture arrays).
    void writeImageArray(VkDescriptorSet set, uint32_t binding,
                         const std::vector<VkDescriptorImageInfo>& imageInfos,
                         VkDescriptorType type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    // Destroy all managed layouts and the pool.
    void cleanup();

private:
    VkDevice device;
    std::vector<VkDescriptorSetLayout> layouts;
    VkDescriptorPool pool = VK_NULL_HANDLE;
};
