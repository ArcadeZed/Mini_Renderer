#pragma once

#include "Mesh.h"
#include <string>
#include <ctime>
#include "../material/Material.h"
#include <unordered_map>
#include <functional>

struct VertexKey {
    glm::ivec3 pos;
    glm::ivec3 normal;
    glm::ivec2 uv;
    glm::ivec3 color;

    bool operator==(const VertexKey& other) const {
        return pos == other.pos &&
               normal == other.normal &&
               uv == other.uv &&
               color == other.color;
    }
};

constexpr float POS_SCALE    = 10000.0f;
constexpr float NORMAL_SCALE = 32767.0f;
constexpr float UV_SCALE     = 10000.0f;
constexpr float COLOR_SCALE  = 255.0f;

inline glm::ivec3 q3(const glm::vec3& v, float s) {
    return glm::ivec3(glm::round(v * s));
}

inline glm::ivec2 q2(const glm::vec2& v, float s) {
    return glm::ivec2(glm::round(v * s));
}

inline void hash_combine(std::size_t& seed, std::size_t value) {
    seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std {
    template<>
    struct hash<VertexKey> {
        size_t operator()(const VertexKey& k) const {
            size_t seed = 0;
            auto h = std::hash<int>{};

            hash_combine(seed, h(k.pos.x));
            hash_combine(seed, h(k.pos.y));
            hash_combine(seed, h(k.pos.z));

            hash_combine(seed, h(k.normal.x));
            hash_combine(seed, h(k.normal.y));
            hash_combine(seed, h(k.normal.z));

            hash_combine(seed, h(k.uv.x));
            hash_combine(seed, h(k.uv.y));

            hash_combine(seed, h(k.color.x));
            hash_combine(seed, h(k.color.y));
            hash_combine(seed, h(k.color.z));

            return seed;
        }
    };
}

// Static utility for loading mesh data from files.
// Supports:
//   - .obj format (Wavefront OBJ, using tinyobjloader)
//   - .txt format (custom VERTICES/INDICES sections)
class MeshLoader {
public:
    // Load vertices + indices from a mesh file.
    // Auto-detects format based on file extension.
    // Returns false if file could not be opened or parsed.
    static bool loadFromFile(const std::string& filepath,
                             std::vector<Vertex>& outVertices,
                             std::vector<uint32_t>& outIndices);

    // Load mesh with material information (for multi-material .obj files)
    // Returns false if file could not be opened or parsed.
    // If .mtl file is present, loads materials and creates submeshes.
    // If no .mtl file, creates single submesh with default material.
    static bool loadFromFileWithMaterials(const std::string& filepath,
                                          std::vector<Vertex>& outVertices,
                                          std::vector<uint32_t>& outIndices,
                                          std::vector<SubMesh>& outSubMeshes,
                                          std::vector<Material>& outMaterials);

    // Resolve file path, trying alternative locations.
    // Returns the resolved path, or empty string if not found.
    static std::string resolvePath(const std::string& filepath);

    // Get file modification time. Returns 0 if file not found.
    static std::time_t getModTime(const std::string& filepath);
    static std::vector<Material> loadMTL(const std::string& mtlPath, 
                                          const std::string& baseDir);
};
