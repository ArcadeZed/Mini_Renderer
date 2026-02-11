#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

// Forward declaration (avoid pulling in entire tiny_gltf.h in header)
namespace tinygltf {
    class Model;
}

struct GLTFMeshData {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    std::vector<glm::vec4> tangents;  // w component = handedness (+1 or -1)
    std::vector<uint32_t> indices;

    int materialIndex = -1;
    std::string name;

    // Node transform from glTF scene hierarchy
    glm::mat4 transform = glm::mat4(1.0f);
};

struct GLTFMaterial {
    // PBR Metallic-Roughness
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;

    // Texture indices (into GLTFModel::textures)
    int baseColorTextureIndex = -1;
    int metallicRoughnessTextureIndex = -1;  // Combined: G=Roughness, B=Metallic
    int normalTextureIndex = -1;
    float normalScale = 1.0f;

    // Emissive
    glm::vec3 emissiveFactor = glm::vec3(0.0f);
    int emissiveTextureIndex = -1;

    // Alpha mode
    enum class AlphaMode { Alpha_Opaque, Alpha_Mask, Alpha_Blend };
    AlphaMode alphaMode = AlphaMode::Alpha_Opaque;
    float alphaCutoff = 0.5f;

    bool doubleSided = false;
    std::string name;
};

struct GLTFTexture {
    std::string uri;          // Relative path from glTF file
    std::string fullPath;     // Absolute path for loading
    int width = 0;
    int height = 0;
    int channels = 0;
};

struct GLTFModel {
    std::vector<GLTFMeshData> meshes;
    std::vector<GLTFMaterial> materials;
    std::vector<GLTFTexture> textures;

    std::string baseDirectory;  // Directory of the glTF file
};

class Mesh;  // Forward declaration

class GLTFLoader {
public:
    // Load glTF file and extract mesh/material data
    static bool loadGLTF(const std::string& filepath, GLTFModel& outModel);

    // Helper: Convert GLTFMeshData to Mesh with proper Vertex format
    // Note: Does NOT upload to GPU - call mesh.upload() separately
    static void convertToMesh(const GLTFMeshData& gltfMesh, Mesh& outMesh, const glm::vec3& defaultColor = glm::vec3(1.0f));

    // Helper to extract buffer data (public for helper functions)
    template<typename T>
    static void extractAccessorData(const tinygltf::Model& model, int accessorIndex,
                                    std::vector<T>& outData);

private:
    static void extractMeshes(const tinygltf::Model& model, GLTFModel& outModel);
    static void extractMaterials(const tinygltf::Model& model, GLTFModel& outModel);
    static void extractTextures(const tinygltf::Model& model, GLTFModel& outModel);
};
