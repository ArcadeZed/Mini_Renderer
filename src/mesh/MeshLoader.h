#pragma once

#include "Mesh.h"
#include <string>
#include <ctime>
#include "../material/Material.h"

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
