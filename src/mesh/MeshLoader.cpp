#include "MeshLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <sys/stat.h>

std::string MeshLoader::resolvePath(const std::string& filepath) {
    // Try the given path first
    {
        std::ifstream f(filepath);
        if (f.is_open()) return filepath;
    }

    // Try without "../" prefix (for VS Code debugger running from repo root)
    if (filepath.find("../") == 0) {
        std::string alt = filepath.substr(3);
        std::ifstream f(alt);
        if (f.is_open()) return alt;
    } else {
        // Try with "../" prefix (for running from build directory)
        std::string alt = "../" + filepath;
        std::ifstream f(alt);
        if (f.is_open()) return alt;
    }

    return "";
}

bool MeshLoader::loadFromFile(const std::string& filepath,
                               std::vector<Vertex>& outVertices,
                               std::vector<uint16_t>& outIndices) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    outVertices.clear();
    outIndices.clear();

    std::string line;
    bool readingVertices = false;
    bool readingIndices = false;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        if (line.find("VERTICES") != std::string::npos) {
            readingVertices = true;
            readingIndices = false;
            continue;
        }
        if (line.find("INDICES") != std::string::npos) {
            readingVertices = false;
            readingIndices = true;
            continue;
        }

        std::istringstream iss(line);

        if (readingVertices) {
            Vertex v;
            iss >> v.pos.x >> v.pos.y >> v.pos.z
                >> v.color.r >> v.color.g >> v.color.b
                >> v.normal.x >> v.normal.y >> v.normal.z
                >> v.uv.x >> v.uv.y;
            outVertices.push_back(v);
        } else if (readingIndices) {
            uint16_t index;
            while (iss >> index) {
                outIndices.push_back(index);
            }
        }
    }

    std::cout << "Loaded mesh from " << filepath << ": "
              << outVertices.size() << " vertices, "
              << outIndices.size() << " indices" << std::endl;
    return true;
}

std::time_t MeshLoader::getModTime(const std::string& filepath) {
    struct stat fileInfo;
    if (stat(filepath.c_str(), &fileInfo) == 0) {
        return fileInfo.st_mtime;
    }
    return 0;
}
