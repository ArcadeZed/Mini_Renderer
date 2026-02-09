#include "MeshLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <sys/stat.h>
#include <algorithm>
#include "../material/Material.h"
#include <filesystem>

#define TINYOBJLOADER_IMPLEMENTATION
#include "../../external/tiny_obj_loader.h"

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

// Helper: Load .txt format (custom VERTICES/INDICES sections)
static bool loadTXT(const std::string& filepath,
                    std::vector<Vertex>& outVertices,
                    std::vector<uint32_t>& outIndices) {
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
            uint32_t index;
            while (iss >> index) {
                outIndices.push_back(index);
            }
        }
    }

    std::cout << "Loaded .txt mesh from " << filepath << ": "
              << outVertices.size() << " vertices, "
              << outIndices.size() << " indices" << std::endl;
    return true;
}

// Helper: Load .obj format with material support
static bool loadOBJWithMaterials(const std::string& filepath,
                                  std::vector<Vertex>& outVertices,
                                  std::vector<uint32_t>& outIndices,
                                  std::vector<SubMesh>& outSubMeshes,
                                  std::vector<Material>& outMaterials) {
    tinyobj::ObjReaderConfig readerConfig;
    readerConfig.triangulate = true;
    readerConfig.vertex_color = false;

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(filepath, readerConfig)) {
        if (!reader.Error().empty()) {
            std::cerr << "TinyObjReader error: " << reader.Error() << std::endl;
        }
        return false;
    }

    if (!reader.Warning().empty()) {
        std::cout << "TinyObjReader warning: " << reader.Warning() << std::endl;
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();
    const auto& materials = reader.GetMaterials();

    outVertices.clear();
    outIndices.clear();
    outSubMeshes.clear();
    outMaterials.clear();

    // Load materials from .mtl file
    std::string baseDir = filepath.substr(0, filepath.find_last_of("/\\"));
    for (const auto& mat : materials) {
        Material material;
        material.name = mat.name;
        material.ambient = glm::vec3(mat.ambient[0], mat.ambient[1], mat.ambient[2]);
        material.diffuse = glm::vec3(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]);
        material.specular = glm::vec3(mat.specular[0], mat.specular[1], mat.specular[2]);
        material.shininess = mat.shininess;

        if (!mat.diffuse_texname.empty()) {
            std::filesystem::path fullPath = std::filesystem::path(baseDir) / mat.diffuse_texname;
            material.diffuseTexturePath = fullPath.string();
        }

        outMaterials.push_back(material);
    }

    // If no materials found, create default material
    if (outMaterials.empty()) {
        Material defaultMat;
        defaultMat.name = "default";
        outMaterials.push_back(defaultMat);
    }

    // Iterate over shapes
    for (const auto& shape : shapes) {
        size_t indexOffset = 0;

        // Track material changes to create submeshes
        int currentMaterialId = -999;  // Invalid initial value
        uint32_t submeshStartIndex = static_cast<uint32_t>(outIndices.size());

        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            size_t fv = shape.mesh.num_face_vertices[f];

            // Get material ID for this face
            int faceMaterialId = shape.mesh.material_ids[f];

            // If material changed, create new submesh
            if (faceMaterialId != currentMaterialId) {
                // Save previous submesh (if any)
                if (currentMaterialId != -999) {
                    SubMesh submesh;
                    submesh.firstIndex = submeshStartIndex;
                    submesh.indexCount = static_cast<uint32_t>(outIndices.size()) - submeshStartIndex;
                    submesh.materialIndex = (currentMaterialId >= 0) ? currentMaterialId : 0;
                    outSubMeshes.push_back(submesh);
                }

                // Start new submesh
                currentMaterialId = faceMaterialId;
                submeshStartIndex = static_cast<uint32_t>(outIndices.size());
            }

            // Process face vertices
            for (size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];

                Vertex vertex{};
                vertex.pos.x = attrib.vertices[3 * idx.vertex_index + 0];
                vertex.pos.y = attrib.vertices[3 * idx.vertex_index + 1];
                vertex.pos.z = attrib.vertices[3 * idx.vertex_index + 2];

                if (idx.normal_index >= 0) {
                    vertex.normal.x = attrib.normals[3 * idx.normal_index + 0];
                    vertex.normal.y = attrib.normals[3 * idx.normal_index + 1];
                    vertex.normal.z = attrib.normals[3 * idx.normal_index + 2];
                } else {
                    vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }

                if (idx.texcoord_index >= 0) {
                    vertex.uv.x = attrib.texcoords[2 * idx.texcoord_index + 0];
                    vertex.uv.y = 1.0f - attrib.texcoords[2 * idx.texcoord_index + 1];
                } else {
                    vertex.uv = glm::vec2(0.0f, 0.0f);
                }

                vertex.color = glm::vec3(1.0f, 1.0f, 1.0f);

                outVertices.push_back(vertex);
                outIndices.push_back(static_cast<uint32_t>(outVertices.size() - 1));
            }

            indexOffset += fv;
        }

        // Don't forget the last submesh!
        if (currentMaterialId != -999) {
            SubMesh submesh;
            submesh.firstIndex = submeshStartIndex;
            submesh.indexCount = static_cast<uint32_t>(outIndices.size()) - submeshStartIndex;
            submesh.materialIndex = (currentMaterialId >= 0) ? currentMaterialId : 0;
            outSubMeshes.push_back(submesh);
        }
    }

    std::cout << "Loaded .obj mesh from " << filepath << ": "
              << outVertices.size() << " vertices, "
              << outIndices.size() << " indices, "
              << outSubMeshes.size() << " submeshes, "
              << outMaterials.size() << " materials" << std::endl;
    return true;
}

// Helper: Load .obj format (Wavefront OBJ using tinyobjloader) - Simple version without materials
static bool loadOBJ(const std::string& filepath,
                    std::vector<Vertex>& outVertices,
                    std::vector<uint32_t>& outIndices) {
    tinyobj::ObjReaderConfig readerConfig;
    readerConfig.triangulate = true;  // Convert to triangles

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(filepath, readerConfig)) {
        if (!reader.Error().empty()) {
            std::cerr << "TinyObjReader error: " << reader.Error() << std::endl;
        }
        return false;
    }

    if (!reader.Warning().empty()) {
        std::cout << "TinyObjReader warning: " << reader.Warning() << std::endl;
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();

    outVertices.clear();
    outIndices.clear();

    // Iterate over shapes (meshes)
    for (const auto& shape : shapes) {
        size_t indexOffset = 0;

        // Iterate over faces (triangles after triangulation)
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            size_t fv = shape.mesh.num_face_vertices[f];

            // Iterate over vertices in the face
            for (size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];

                Vertex vertex{};

                // Position
                vertex.pos.x = attrib.vertices[3 * idx.vertex_index + 0];
                vertex.pos.y = attrib.vertices[3 * idx.vertex_index + 1];
                vertex.pos.z = attrib.vertices[3 * idx.vertex_index + 2];

                // Normal (if available)
                if (idx.normal_index >= 0) {
                    vertex.normal.x = attrib.normals[3 * idx.normal_index + 0];
                    vertex.normal.y = attrib.normals[3 * idx.normal_index + 1];
                    vertex.normal.z = attrib.normals[3 * idx.normal_index + 2];
                } else {
                    // Default normal if not provided
                    vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }

                // UV (if available)
                if (idx.texcoord_index >= 0) {
                    vertex.uv.x = attrib.texcoords[2 * idx.texcoord_index + 0];
                    vertex.uv.y = 1.0f - attrib.texcoords[2 * idx.texcoord_index + 1];  // Flip Y for Vulkan
                } else {
                    vertex.uv = glm::vec2(0.0f, 0.0f);
                }

                // Vertex color (default white, since OBJ doesn't have per-vertex colors usually)
                vertex.color = glm::vec3(1.0f, 1.0f, 1.0f);

                outVertices.push_back(vertex);
                outIndices.push_back(static_cast<uint32_t>(outVertices.size() - 1));
            }

            indexOffset += fv;
        }
    }

    std::cout << "Loaded .obj mesh from " << filepath << ": "
              << outVertices.size() << " vertices, "
              << outIndices.size() << " indices" << std::endl;
    return true;
}

bool MeshLoader::loadFromFile(const std::string& filepath,
                               std::vector<Vertex>& outVertices,
                               std::vector<uint32_t>& outIndices) {
    // Detect file extension
    std::string ext;
    size_t dotPos = filepath.find_last_of('.');
    if (dotPos != std::string::npos) {
        ext = filepath.substr(dotPos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    // Route to appropriate loader
    if (ext == ".obj") {
        return loadOBJ(filepath, outVertices, outIndices);
    } else if (ext == ".txt") {
        return loadTXT(filepath, outVertices, outIndices);
    } else {
        std::cerr << "Unknown mesh file format: " << filepath << std::endl;
        return false;
    }
}

bool MeshLoader::loadFromFileWithMaterials(const std::string& filepath,
                                            std::vector<Vertex>& outVertices,
                                            std::vector<uint32_t>& outIndices,
                                            std::vector<SubMesh>& outSubMeshes,
                                            std::vector<Material>& outMaterials) {
    // Detect file extension
    std::string ext;
    size_t dotPos = filepath.find_last_of('.');
    if (dotPos != std::string::npos) {
        ext = filepath.substr(dotPos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    // Route to appropriate loader
    if (ext == ".obj") {
        return loadOBJWithMaterials(filepath, outVertices, outIndices, outSubMeshes, outMaterials);
    } else if (ext == ".txt") {
        // Load .txt format (no materials), create single submesh
        bool success = loadTXT(filepath, outVertices, outIndices);
        if (success) {
            // Create single submesh covering all indices
            SubMesh submesh;
            submesh.firstIndex = 0;
            submesh.indexCount = static_cast<uint32_t>(outIndices.size());
            submesh.materialIndex = 0;
            outSubMeshes.push_back(submesh);

            // Create default material
            Material defaultMat;
            defaultMat.name = "default";
            outMaterials.push_back(defaultMat);
        }
        return success;
    } else {
        std::cerr << "Unknown mesh file format: " << filepath << std::endl;
        return false;
    }
}

std::time_t MeshLoader::getModTime(const std::string& filepath) {
    struct stat fileInfo;
    if (stat(filepath.c_str(), &fileInfo) == 0) {
        return fileInfo.st_mtime;
    }
    return 0;
}

std::vector<Material> MeshLoader::loadMTL(const std::string& mtlPath,
                                           const std::string& baseDir) {
    std::vector<Material> materials;
    std::ifstream file(mtlPath);

    if (!file.is_open()) {
        std::cerr << "Failed to open .mtl file: " << mtlPath << std::endl;
        return materials;  // Empty vector
    }

    Material currentMaterial;  // Temporarily holds material being parsed
    bool hasMaterial = false;  // Track if we've started reading a material

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;

        if (keyword == "newmtl") {
            // If we were already reading a material, save it before starting a new one
            if (hasMaterial) {
                materials.push_back(currentMaterial);
            }

            // Start a new material
            currentMaterial = Material();  // Reset to default
            iss >> currentMaterial.name;
            hasMaterial = true;

        } else if (keyword == "Ka") {
            // Parse Ambient color (3 floats)
            iss >> currentMaterial.ambient.x >> currentMaterial.ambient.y >> currentMaterial.ambient.z;

        } else if (keyword == "Kd") {
            // Parse Diffuse color (3 floats)
            iss >> currentMaterial.diffuse.x >> currentMaterial.diffuse.y >> currentMaterial.diffuse.z;

        } else if (keyword == "Ks") {
            // Parse Specular color (3 floats)
            iss >> currentMaterial.specular.x >> currentMaterial.specular.y >> currentMaterial.specular.z;

        } else if (keyword == "Ns") {
            // Parse Shininess (1 float)
            iss >> currentMaterial.shininess;

        } else if (keyword == "map_Kd") {
            // Parse Diffuse texture filename
            std::string filename;
            iss >> filename;

            // Combine baseDir + filename using std::filesystem
            std::filesystem::path fullPath = std::filesystem::path(baseDir) / filename;
            currentMaterial.diffuseTexturePath = fullPath.string();
        }
    }

    // Don't forget to add the last material!
    if (hasMaterial) {
        materials.push_back(currentMaterial);
    }

    std::cout << "Loaded " << materials.size() << " materials from " << mtlPath << std::endl;
    return materials;
}
