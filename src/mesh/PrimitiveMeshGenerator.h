#pragma once

#include "Mesh.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>
#include <cmath>

class PrimitiveMeshGenerator {
public:
    // Generate UV sphere (latitude/longitude segments)
    static void generateSphere(std::vector<Vertex>& outVertices,
                                std::vector<uint32_t>& outIndices,
                                float radius = 1.0f,
                                uint32_t latSegments = 32,
                                uint32_t longSegments = 64) {
        outVertices.clear();
        outIndices.clear();

        const glm::vec3 white(1.0f, 1.0f, 1.0f);

        // Generate vertices
        for (uint32_t lat = 0; lat <= latSegments; ++lat) {
            float theta = static_cast<float>(lat) * glm::pi<float>() / static_cast<float>(latSegments);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            for (uint32_t lon = 0; lon <= longSegments; ++lon) {
                float phi = static_cast<float>(lon) * 2.0f * glm::pi<float>() / static_cast<float>(longSegments);
                float sinPhi = std::sin(phi);
                float cosPhi = std::cos(phi);

                // Position (spherical coordinates)
                glm::vec3 position;
                position.x = radius * cosPhi * sinTheta;
                position.y = radius * cosTheta;
                position.z = radius * sinPhi * sinTheta;

                // Normal (normalized position for unit sphere)
                glm::vec3 normal = glm::normalize(position);

                // UV coordinates
                glm::vec2 uv;
                uv.x = static_cast<float>(lon) / static_cast<float>(longSegments);
                uv.y = static_cast<float>(lat) / static_cast<float>(latSegments);

                outVertices.push_back({position, white, normal, uv});
            }
        }

        // Generate indices (two triangles per quad, CCW winding)
        for (uint32_t lat = 0; lat < latSegments; ++lat) {
            for (uint32_t lon = 0; lon < longSegments; ++lon) {
                uint32_t first = lat * (longSegments + 1) + lon;
                uint32_t second = first + longSegments + 1;

                // First triangle (CCW from outside: first, first+1, second)
                outIndices.push_back(first);
                outIndices.push_back(first + 1);
                outIndices.push_back(second);

                // Second triangle (CCW from outside: first+1, second+1, second)
                outIndices.push_back(first + 1);
                outIndices.push_back(second + 1);
                outIndices.push_back(second);
            }
        }
    }

    // Generate cube with proper normals (24 vertices, 6 faces)
    static void generateCube(std::vector<Vertex>& outVertices,
                             std::vector<uint32_t>& outIndices,
                             float size = 1.0f) {
        outVertices.clear();
        outIndices.clear();

        const glm::vec3 white(1.0f, 1.0f, 1.0f);
        float halfSize = size * 0.5f;

        // Define 6 faces with 4 vertices each (24 vertices total)
        // Each face has its own normal for flat shading

        // Front face (+Z)
        outVertices.push_back({{-halfSize, -halfSize,  halfSize}, white, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
        outVertices.push_back({{ halfSize, -halfSize,  halfSize}, white, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}});
        outVertices.push_back({{ halfSize,  halfSize,  halfSize}, white, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}});
        outVertices.push_back({{-halfSize,  halfSize,  halfSize}, white, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}});

        // Back face (-Z)
        outVertices.push_back({{ halfSize, -halfSize, -halfSize}, white, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}});
        outVertices.push_back({{-halfSize, -halfSize, -halfSize}, white, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}});
        outVertices.push_back({{-halfSize,  halfSize, -halfSize}, white, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}});
        outVertices.push_back({{ halfSize,  halfSize, -halfSize}, white, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}});

        // Right face (+X)
        outVertices.push_back({{ halfSize, -halfSize,  halfSize}, white, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
        outVertices.push_back({{ halfSize, -halfSize, -halfSize}, white, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}});
        outVertices.push_back({{ halfSize,  halfSize, -halfSize}, white, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}});
        outVertices.push_back({{ halfSize,  halfSize,  halfSize}, white, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}});

        // Left face (-X)
        outVertices.push_back({{-halfSize, -halfSize, -halfSize}, white, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
        outVertices.push_back({{-halfSize, -halfSize,  halfSize}, white, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}});
        outVertices.push_back({{-halfSize,  halfSize,  halfSize}, white, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}});
        outVertices.push_back({{-halfSize,  halfSize, -halfSize}, white, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}});

        // Top face (+Y)
        outVertices.push_back({{-halfSize,  halfSize,  halfSize}, white, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
        outVertices.push_back({{ halfSize,  halfSize,  halfSize}, white, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}});
        outVertices.push_back({{ halfSize,  halfSize, -halfSize}, white, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}});
        outVertices.push_back({{-halfSize,  halfSize, -halfSize}, white, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}});

        // Bottom face (-Y)
        outVertices.push_back({{-halfSize, -halfSize, -halfSize}, white, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}});
        outVertices.push_back({{ halfSize, -halfSize, -halfSize}, white, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}});
        outVertices.push_back({{ halfSize, -halfSize,  halfSize}, white, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}});
        outVertices.push_back({{-halfSize, -halfSize,  halfSize}, white, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}});

        // Generate indices (2 triangles per face, CCW winding)
        for (uint32_t face = 0; face < 6; ++face) {
            uint32_t baseIndex = face * 4;

            // First triangle
            outIndices.push_back(baseIndex + 0);
            outIndices.push_back(baseIndex + 1);
            outIndices.push_back(baseIndex + 2);

            // Second triangle
            outIndices.push_back(baseIndex + 0);
            outIndices.push_back(baseIndex + 2);
            outIndices.push_back(baseIndex + 3);
        }
    }

    // Generate XZ plane (Y-up, centered at origin)
    static void generatePlane(std::vector<Vertex>& outVertices,
                              std::vector<uint32_t>& outIndices,
                              float width = 10.0f,
                              float depth = 10.0f,
                              uint32_t subdivisions = 1) {
        outVertices.clear();
        outIndices.clear();

        const glm::vec3 white(1.0f, 1.0f, 1.0f);
        const glm::vec3 up(0.0f, 1.0f, 0.0f);  // Normal pointing up (+Y)

        float halfWidth = width * 0.5f;
        float halfDepth = depth * 0.5f;

        // Generate vertices in grid
        for (uint32_t z = 0; z <= subdivisions; ++z) {
            for (uint32_t x = 0; x <= subdivisions; ++x) {
                float xPos = -halfWidth + (static_cast<float>(x) / static_cast<float>(subdivisions)) * width;
                float zPos = -halfDepth + (static_cast<float>(z) / static_cast<float>(subdivisions)) * depth;

                glm::vec3 position(xPos, 0.0f, zPos);

                glm::vec2 uv(
                    static_cast<float>(x) / static_cast<float>(subdivisions),
                    static_cast<float>(z) / static_cast<float>(subdivisions)
                );

                outVertices.push_back({position, white, up, uv});
            }
        }

        // Generate indices (two triangles per quad, CCW from above)
        for (uint32_t z = 0; z < subdivisions; ++z) {
            for (uint32_t x = 0; x < subdivisions; ++x) {
                uint32_t topLeft = z * (subdivisions + 1) + x;
                uint32_t topRight = topLeft + 1;
                uint32_t bottomLeft = (z + 1) * (subdivisions + 1) + x;
                uint32_t bottomRight = bottomLeft + 1;

                // First triangle (CCW from above: top-left, bottom-left, top-right)
                outIndices.push_back(topLeft);
                outIndices.push_back(bottomLeft);
                outIndices.push_back(topRight);

                // Second triangle (CCW from above: top-right, bottom-left, bottom-right)
                outIndices.push_back(topRight);
                outIndices.push_back(bottomLeft);
                outIndices.push_back(bottomRight);
            }
        }
    }
};
