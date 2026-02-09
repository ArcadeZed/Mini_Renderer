#pragma once

#include "Mesh.h"
#include <string>
#include <ctime>

// Static utility for loading mesh data from files.
// Supports .txt format with VERTICES/INDICES sections.
class MeshLoader {
public:
    // Load vertices + indices from a mesh file.
    // Returns false if file could not be opened.
    static bool loadFromFile(const std::string& filepath,
                             std::vector<Vertex>& outVertices,
                             std::vector<uint16_t>& outIndices);

    // Resolve file path, trying alternative locations.
    // Returns the resolved path, or empty string if not found.
    static std::string resolvePath(const std::string& filepath);

    // Get file modification time. Returns 0 if file not found.
    static std::time_t getModTime(const std::string& filepath);
};
