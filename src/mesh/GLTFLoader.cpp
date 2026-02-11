#define TINYGLTF_IMPLEMENTATION

// STB_IMAGE_IMPLEMENTATION handling:
// - For test_gltf_loader: Define it here (no RenderEngine)
// - For main project: Already defined in RenderEngine.cpp
#ifdef TEST_GLTF_LOADER
#define STB_IMAGE_IMPLEMENTATION
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

#include "GLTFLoader.h"
#include "Mesh.h"
#include <iostream>
#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace fs = std::filesystem;

namespace {
    // Helper: Extract local transform matrix from a glTF node
    glm::mat4 getNodeLocalTransform(const tinygltf::Node& node) {
        glm::mat4 transform = glm::mat4(1.0f);

        if (node.matrix.size() == 16) {
            // Node has a 4x4 transformation matrix (column-major)
            transform = glm::make_mat4(node.matrix.data());
        } else {
            // Decomposed TRS (Translation, Rotation, Scale)
            glm::vec3 translation(0.0f);
            glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Identity quaternion
            glm::vec3 scale(1.0f);

            if (node.translation.size() == 3) {
                translation = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
            }

            if (node.rotation.size() == 4) {
                // glTF quaternion format: [x, y, z, w]
                // glm::quat constructor: quat(w, x, y, z)
                rotation = glm::quat(
                    static_cast<float>(node.rotation[3]),  // w
                    static_cast<float>(node.rotation[0]),  // x
                    static_cast<float>(node.rotation[1]),  // y
                    static_cast<float>(node.rotation[2])   // z
                );
            }

            if (node.scale.size() == 3) {
                scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
            }

            // Compose transform: T * R * S
            transform = glm::translate(glm::mat4(1.0f), translation);
            transform *= glm::mat4_cast(rotation);
            transform = glm::scale(transform, scale);
        }

        return transform;
    }

    // Helper: Extract mesh primitive data (positions, normals, UVs, indices)
    void extractPrimitiveData(const tinygltf::Model& model,
                              const tinygltf::Primitive& primitive,
                              GLTFMeshData& meshData) {
        // Positions (required)
        if (primitive.attributes.count("POSITION")) {
            GLTFLoader::extractAccessorData(model, primitive.attributes.at("POSITION"), meshData.positions);
        }

        // Normals (required for lighting)
        if (primitive.attributes.count("NORMAL")) {
            GLTFLoader::extractAccessorData(model, primitive.attributes.at("NORMAL"), meshData.normals);
        }

        // TexCoords (UV)
        if (primitive.attributes.count("TEXCOORD_0")) {
            GLTFLoader::extractAccessorData(model, primitive.attributes.at("TEXCOORD_0"), meshData.texCoords);
        }

        // Tangents (for normal mapping)
        if (primitive.attributes.count("TANGENT")) {
            GLTFLoader::extractAccessorData(model, primitive.attributes.at("TANGENT"), meshData.tangents);
        }

        // Indices
        if (primitive.indices >= 0) {
            const auto& accessor = model.accessors[primitive.indices];
            const auto& bufferView = model.bufferViews[accessor.bufferView];
            const auto& buffer = model.buffers[bufferView.buffer];

            const uint8_t* dataPtr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;

            meshData.indices.resize(accessor.count);

            // Handle different index types
            if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                const uint16_t* indices16 = reinterpret_cast<const uint16_t*>(dataPtr);
                for (size_t i = 0; i < accessor.count; ++i) {
                    meshData.indices[i] = static_cast<uint32_t>(indices16[i]);
                }
            } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                const uint32_t* indices32 = reinterpret_cast<const uint32_t*>(dataPtr);
                std::copy(indices32, indices32 + accessor.count, meshData.indices.begin());
            } else {
                std::cerr << "[GLTFLoader] Unsupported index component type" << std::endl;
            }
        }
    }

    // Recursive node traversal
    void processNode(const tinygltf::Model& model,
                     int nodeIndex,
                     const glm::mat4& parentTransform,
                     GLTFModel& outModel) {
        if (nodeIndex < 0 || nodeIndex >= model.nodes.size()) {
            return;
        }

        const auto& node = model.nodes[nodeIndex];

        // Get this node's local transform
        glm::mat4 localTransform = getNodeLocalTransform(node);

        // Accumulate: parent * local
        glm::mat4 worldTransform = parentTransform * localTransform;

        // If this node has a mesh, extract all primitives
        if (node.mesh >= 0 && node.mesh < model.meshes.size()) {
            const auto& mesh = model.meshes[node.mesh];

            for (const auto& primitive : mesh.primitives) {
                GLTFMeshData meshData;
                meshData.name = mesh.name.empty() ? ("Node_" + std::to_string(nodeIndex)) : mesh.name;
                meshData.materialIndex = primitive.material;
                meshData.transform = worldTransform;  // Store accumulated transform

                // Extract vertex data
                extractPrimitiveData(model, primitive, meshData);

                std::cout << "  Mesh '" << meshData.name << "': "
                          << meshData.positions.size() << " vertices, "
                          << meshData.indices.size() << " indices"
                          << " (node " << nodeIndex << ")" << std::endl;

                outModel.meshes.push_back(meshData);
            }
        }

        // Recursively process children
        for (int childIndex : node.children) {
            processNode(model, childIndex, worldTransform, outModel);
        }
    }
}

bool GLTFLoader::loadGLTF(const std::string& filepath, GLTFModel& outModel) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;

    // Determine file type (.gltf = ASCII, .glb = Binary)
    bool isGLB = filepath.find(".glb") != std::string::npos;
    bool ret = false;

    if (isGLB) {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
    } else {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
    }

    if (!warn.empty()) {
        std::cout << "[GLTFLoader] Warning: " << warn << std::endl;
    }

    if (!err.empty()) {
        std::cerr << "[GLTFLoader] Error: " << err << std::endl;
        return false;
    }

    if (!ret) {
        std::cerr << "[GLTFLoader] Failed to load glTF: " << filepath << std::endl;
        return false;
    }

    std::cout << "[GLTFLoader] Successfully loaded: " << filepath << std::endl;
    std::cout << "  Meshes: " << model.meshes.size() << std::endl;
    std::cout << "  Materials: " << model.materials.size() << std::endl;
    std::cout << "  Textures: " << model.textures.size() << std::endl;

    // Store base directory for texture loading
    outModel.baseDirectory = fs::path(filepath).parent_path().string();

    // Extract data
    extractTextures(model, outModel);
    extractMaterials(model, outModel);
    extractMeshes(model, outModel);

    return true;
}

void GLTFLoader::extractMeshes(const tinygltf::Model& model, GLTFModel& outModel) {
    if (model.scenes.empty()) {
        std::cerr << "[GLTFLoader] No scenes in glTF file!" << std::endl;
        return;
    }

    // Use the default scene (usually index 0)
    int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    const auto& scene = model.scenes[sceneIndex];

    std::cout << "[GLTFLoader] Processing scene '" << scene.name << "' with "
              << scene.nodes.size() << " root nodes" << std::endl;

    // Traverse each root node with identity transform
    glm::mat4 identity = glm::mat4(1.0f);
    for (int nodeIndex : scene.nodes) {
        processNode(model, nodeIndex, identity, outModel);
    }

    std::cout << "[GLTFLoader] Extracted " << outModel.meshes.size()
              << " mesh primitives from scene hierarchy" << std::endl;
}

void GLTFLoader::extractMaterials(const tinygltf::Model& model, GLTFModel& outModel) {
    for (const auto& mat : model.materials) {
        GLTFMaterial material;
        material.name = mat.name;

        // Check for KHR_materials_pbrSpecularGlossiness extension
        bool hasSpecGloss = mat.extensions.find("KHR_materials_pbrSpecularGlossiness") != mat.extensions.end();

        if (hasSpecGloss) {
            // Use Specular-Glossiness workflow (convert to Metallic-Roughness)
            const auto& ext = mat.extensions.at("KHR_materials_pbrSpecularGlossiness");

            // Diffuse → Base Color
            if (ext.Has("diffuseFactor")) {
                auto diffuse = ext.Get("diffuseFactor");
                material.baseColorFactor = glm::vec4(
                    diffuse.Get(0).GetNumberAsDouble(),
                    diffuse.Get(1).GetNumberAsDouble(),
                    diffuse.Get(2).GetNumberAsDouble(),
                    diffuse.Get(3).GetNumberAsDouble()
                );
            }
            if (ext.Has("diffuseTexture")) {
                material.baseColorTextureIndex = ext.Get("diffuseTexture").Get("index").GetNumberAsInt();
            }

            // Specular-Glossiness → Metallic-Roughness (approximate conversion)
            // For now: treat as non-metallic with medium roughness
            material.metallicFactor = 0.0f;  // Dielectrics
            if (ext.Has("glossinessFactor")) {
                float glossiness = static_cast<float>(ext.Get("glossinessFactor").GetNumberAsDouble());
                material.roughnessFactor = 1.0f - glossiness;  // Roughness = 1 - Glossiness
            } else {
                material.roughnessFactor = 0.5f;
            }
            if (ext.Has("specularGlossinessTexture")) {
                material.metallicRoughnessTextureIndex = ext.Get("specularGlossinessTexture").Get("index").GetNumberAsInt();
            }
        } else {
            // Standard PBR Metallic-Roughness
            const auto& pbr = mat.pbrMetallicRoughness;

            // Base Color
            material.baseColorFactor = glm::vec4(
                pbr.baseColorFactor[0],
                pbr.baseColorFactor[1],
                pbr.baseColorFactor[2],
                pbr.baseColorFactor[3]
            );
            material.baseColorTextureIndex = pbr.baseColorTexture.index;

            // Metallic + Roughness
            material.metallicFactor = static_cast<float>(pbr.metallicFactor);
            material.roughnessFactor = static_cast<float>(pbr.roughnessFactor);
            material.metallicRoughnessTextureIndex = pbr.metallicRoughnessTexture.index;
        }

        // Normal Map
        material.normalTextureIndex = mat.normalTexture.index;
        material.normalScale = static_cast<float>(mat.normalTexture.scale);

        // Emissive
        material.emissiveFactor = glm::vec3(
            mat.emissiveFactor[0],
            mat.emissiveFactor[1],
            mat.emissiveFactor[2]
        );
        material.emissiveTextureIndex = mat.emissiveTexture.index;

        // Alpha mode
        if (mat.alphaMode == "OPAQUE") {
            material.alphaMode = GLTFMaterial::AlphaMode::Alpha_Opaque;
        } else if (mat.alphaMode == "MASK") {
            material.alphaMode = GLTFMaterial::AlphaMode::Alpha_Mask;
        } else if (mat.alphaMode == "BLEND") {
            material.alphaMode = GLTFMaterial::AlphaMode::Alpha_Blend;
        }
        material.alphaCutoff = static_cast<float>(mat.alphaCutoff);
        material.doubleSided = mat.doubleSided;

        std::cout << "  Material '" << material.name << "': "
                  << "BaseColor=" << material.baseColorTextureIndex
                  << ", Normal=" << material.normalTextureIndex
                  << ", MetallicRoughness=" << material.metallicRoughnessTextureIndex
                  << std::endl;

        outModel.materials.push_back(material);
    }
}

void GLTFLoader::extractTextures(const tinygltf::Model& model, GLTFModel& outModel) {
    for (const auto& texture : model.textures) {
        GLTFTexture tex;

        if (texture.source >= 0 && texture.source < model.images.size()) {
            const auto& image = model.images[texture.source];
            tex.uri = image.uri;
            tex.fullPath = (fs::path(outModel.baseDirectory) / image.uri).string();
            tex.width = image.width;
            tex.height = image.height;
            tex.channels = image.component;

            std::cout << "  Texture: " << tex.uri << " (" << tex.width << "x" << tex.height << ")" << std::endl;
        }

        outModel.textures.push_back(tex);
    }
}

template<typename T>
void GLTFLoader::extractAccessorData(const tinygltf::Model& model, int accessorIndex,
                                     std::vector<T>& outData) {
    if (accessorIndex < 0) return;

    const auto& accessor = model.accessors[accessorIndex];
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[bufferView.buffer];

    const uint8_t* dataPtr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
    const size_t stride = accessor.ByteStride(bufferView);

    outData.resize(accessor.count);

    if (stride == sizeof(T)) {
        // Tightly packed - direct copy
        std::memcpy(outData.data(), dataPtr, accessor.count * sizeof(T));
    } else {
        // Interleaved or non-standard stride
        for (size_t i = 0; i < accessor.count; ++i) {
            std::memcpy(&outData[i], dataPtr + i * stride, sizeof(T));
        }
    }
}

// Explicit template instantiations
template void GLTFLoader::extractAccessorData<glm::vec2>(const tinygltf::Model&, int, std::vector<glm::vec2>&);
template void GLTFLoader::extractAccessorData<glm::vec3>(const tinygltf::Model&, int, std::vector<glm::vec3>&);
template void GLTFLoader::extractAccessorData<glm::vec4>(const tinygltf::Model&, int, std::vector<glm::vec4>&);

void GLTFLoader::convertToMesh(const GLTFMeshData& gltfMesh, Mesh& outMesh, const glm::vec3& defaultColor) {
    std::vector<Vertex> vertices;
    vertices.reserve(gltfMesh.positions.size());

    // Convert from separate arrays to interleaved Vertex format
    for (size_t i = 0; i < gltfMesh.positions.size(); ++i) {
        Vertex v;
        v.pos = gltfMesh.positions[i];
        v.color = defaultColor;

        // Normal (default to up if not present)
        v.normal = (i < gltfMesh.normals.size()) ? gltfMesh.normals[i] : glm::vec3(0.0f, 1.0f, 0.0f);

        // UV (default to (0,0) if not present)
        v.uv = (i < gltfMesh.texCoords.size()) ? gltfMesh.texCoords[i] : glm::vec2(0.0f);

        // Tangent (default to (1,0,0,1) if not present)
        v.tangent = (i < gltfMesh.tangents.size()) ? gltfMesh.tangents[i] : glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

        vertices.push_back(v);
    }

    // Set geometry (no submeshes - single material mesh)
    outMesh.setGeometry(vertices, gltfMesh.indices);
}
