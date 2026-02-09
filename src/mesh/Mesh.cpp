#include "Mesh.h"
#include "../core/VulkanResource.h"
#include "../core/VulkanCommand.h"
#include <cstring>
#include <iostream>

void Mesh::setGeometry(std::vector<Vertex> verts, std::vector<uint16_t> inds) {
    vertices = std::move(verts);
    indices = std::move(inds);
}

void Mesh::uploadBuffer(VkDevice device, VulkanResource& resource, VulkanCommand& command,
                         const void* data, VkDeviceSize size, VkBufferUsageFlags usage,
                         VkBuffer& outBuffer, VkDeviceMemory& outMemory) {
    VkBuffer staging;
    VkDeviceMemory stagingMemory;
    resource.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          staging, stagingMemory);

    void* mapped;
    vkMapMemory(device, stagingMemory, 0, size, 0, &mapped);
    memcpy(mapped, data, static_cast<size_t>(size));
    vkUnmapMemory(device, stagingMemory);

    resource.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, outBuffer, outMemory);

    VkCommandBuffer cmd = command.beginSingleTimeCommands();
    VkBufferCopy region{};
    region.size = size;
    vkCmdCopyBuffer(cmd, staging, outBuffer, 1, &region);
    command.endSingleTimeCommands(cmd);

    vkDestroyBuffer(device, staging, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
}

void Mesh::upload(VkDevice device, VulkanResource& resource, VulkanCommand& command) {
    uploadBuffer(device, resource, command,
                 vertices.data(), sizeof(Vertex) * vertices.size(),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer, vertexBufferMemory);

    uploadBuffer(device, resource, command,
                 indices.data(), sizeof(uint16_t) * indices.size(),
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer, indexBufferMemory);
}

void Mesh::bind(VkCommandBuffer cmd) const {
    VkBuffer buffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
}

void Mesh::draw(VkCommandBuffer cmd) const {
    vkCmdDrawIndexed(cmd, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
}

void Mesh::cleanup(VkDevice device) {
    if (indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);
        indexBuffer = VK_NULL_HANDLE;
        indexBufferMemory = VK_NULL_HANDLE;
    }
    if (vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        vertexBuffer = VK_NULL_HANDLE;
        vertexBufferMemory = VK_NULL_HANDLE;
    }
}
