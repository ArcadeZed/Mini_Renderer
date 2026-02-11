#pragma once

#include <vulkan/vulkan.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <vector>
#include <array>
#include "../material/Material.h"

class VulkanResource;
class VulkanCommand;

// Represents a subset of a mesh that uses a single material
struct SubMesh {
    uint32_t firstIndex;    // Starting index in the index buffer
    uint32_t indexCount;    // Number of indices for this submesh
    uint32_t materialIndex; // Index into Mesh's materials vector
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 tangent;  // xyz = tangent direction, w = handedness (+1 or -1)

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription desc{};
        desc.binding = 0;
        desc.stride = sizeof(Vertex);
        desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return desc;
    }

    static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 5> attrs{};
        attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)};
        attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)};
        attrs[2] = {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)};
        attrs[3] = {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)};
        attrs[4] = {4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, tangent)};
        return attrs;
    }
};

// Owns vertex + index data and their GPU buffers.
// Upload transfers data to device-local memory via staging buffers.
// Supports multiple materials via SubMeshes.
class Mesh {
public:
    void setGeometry(std::vector<Vertex> vertices, std::vector<uint32_t> indices);

    // Set geometry with material information (for multi-material meshes)
    void setGeometryWithMaterials(std::vector<Vertex> vertices,
                                   std::vector<uint32_t> indices,
                                   std::vector<SubMesh> submeshes,
                                   std::vector<Material> materials);

    void upload(VkDevice device, VulkanResource& resource, VulkanCommand& command);
    void bind(VkCommandBuffer cmd) const;
    void draw(VkCommandBuffer cmd) const;

    // Draw a specific submesh (for multi-material rendering)
    void drawSubmesh(VkCommandBuffer cmd, uint32_t submeshIndex) const;

    void cleanup(VkDevice device);

    uint32_t getIndexCount() const { return static_cast<uint32_t>(indices.size()); }
    const std::vector<Vertex>& getVertices() const { return vertices; }
    const std::vector<uint32_t>& getIndices() const { return indices; }
    const std::vector<SubMesh>& getSubMeshes() const { return subMeshes; }
    const std::vector<Material>& getMaterials() const { return materials; }
    bool isUploaded() const { return vertexBuffer != VK_NULL_HANDLE; }
    bool hasMultipleMaterials() const { return !subMeshes.empty(); }

private:
    void uploadBuffer(VkDevice device, VulkanResource& resource, VulkanCommand& command,
                      const void* data, VkDeviceSize size, VkBufferUsageFlags usage,
                      VkBuffer& outBuffer, VkDeviceMemory& outMemory);

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<SubMesh> subMeshes;      // Empty if single-material mesh
    std::vector<Material> materials;      // Empty if single-material mesh

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
};
