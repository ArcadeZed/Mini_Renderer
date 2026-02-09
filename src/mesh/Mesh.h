#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <array>

class VulkanResource;
class VulkanCommand;

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec3 normal;
    glm::vec2 uv;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription desc{};
        desc.binding = 0;
        desc.stride = sizeof(Vertex);
        desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return desc;
    }

    static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 4> attrs{};
        attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)};
        attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)};
        attrs[2] = {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)};
        attrs[3] = {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)};
        return attrs;
    }
};

// Owns vertex + index data and their GPU buffers.
// Upload transfers data to device-local memory via staging buffers.
class Mesh {
public:
    void setGeometry(std::vector<Vertex> vertices, std::vector<uint16_t> indices);
    void upload(VkDevice device, VulkanResource& resource, VulkanCommand& command);
    void bind(VkCommandBuffer cmd) const;
    void draw(VkCommandBuffer cmd) const;
    void cleanup(VkDevice device);

    uint32_t getIndexCount() const { return static_cast<uint32_t>(indices.size()); }
    const std::vector<Vertex>& getVertices() const { return vertices; }
    const std::vector<uint16_t>& getIndices() const { return indices; }
    bool isUploaded() const { return vertexBuffer != VK_NULL_HANDLE; }

private:
    void uploadBuffer(VkDevice device, VulkanResource& resource, VulkanCommand& command,
                      const void* data, VkDeviceSize size, VkBufferUsageFlags usage,
                      VkBuffer& outBuffer, VkDeviceMemory& outMemory);

    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
};
