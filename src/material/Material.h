#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vulkan/vulkan.h>

// Represents a Phong/Blinn-Phong material from a .mtl file
struct Material {
    std::string name;  // Material name (e.g., "09_-_BronzeStatues")

    // Phong material properties (from .mtl file)
    glm::vec3 ambient;   // Ka (Ambient color)
    glm::vec3 diffuse;   // Kd (Diffuse color)
    glm::vec3 specular;  // Ks (Specular color)
    float shininess;     // Ns (Specular exponent)

    // Texture filepath (relative to .obj file)
    std::string diffuseTexturePath;  // map_Kd

    // Vulkan texture resources (loaded at runtime)
    VkImage textureImage = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
    VkImageView textureImageView = VK_NULL_HANDLE;
    uint32_t mipLevels = 1;

    // Default constructor (white material, no texture)
    Material() 
        : name("default"),
          ambient(0.1f, 0.1f, 0.1f),
          diffuse(0.8f, 0.8f, 0.8f),
          specular(1.0f, 1.0f, 1.0f),
          shininess(32.0f),
          diffuseTexturePath("") {}
};