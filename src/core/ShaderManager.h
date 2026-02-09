#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <array>

class ShaderManager {
public:
    explicit ShaderManager(VkDevice device);
    ~ShaderManager();

    // Load a SPIR-V shader module from file, returns cached module if already loaded
    VkShaderModule loadShader(const std::string& filepath);

    // Convenience: load vertex + fragment pair and return pipeline stage infos
    // The returned stage infos reference modules owned by ShaderManager
    std::array<VkPipelineShaderStageCreateInfo, 2> loadVertexFragmentStages(
        const std::string& vertPath, const std::string& fragPath);

    // Destroy all cached shader modules
    void cleanup();

private:
    static std::vector<char> readFile(const std::string& filename);

    VkDevice device;
    std::unordered_map<std::string, VkShaderModule> cache;
};
