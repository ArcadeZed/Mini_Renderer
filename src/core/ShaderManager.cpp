#include "ShaderManager.h"
#include <fstream>
#include <stdexcept>

ShaderManager::ShaderManager(VkDevice device) : device(device) {}

ShaderManager::~ShaderManager() {
    cleanup();
}

std::vector<char> ShaderManager::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();
    return buffer;
}

VkShaderModule ShaderManager::loadShader(const std::string& filepath) {
    // Return cached module if already loaded
    auto it = cache.find(filepath);
    if (it != cache.end()) {
        return it->second;
    }

    auto code = readFile(filepath);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module: " + filepath);
    }

    cache[filepath] = shaderModule;
    return shaderModule;
}

std::array<VkPipelineShaderStageCreateInfo, 2> ShaderManager::loadVertexFragmentStages(
    const std::string& vertPath, const std::string& fragPath) {

    VkShaderModule vertModule = loadShader(vertPath);
    VkShaderModule fragModule = loadShader(fragPath);

    VkPipelineShaderStageCreateInfo vertStageInfo{};
    vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = vertModule;
    vertStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragStageInfo{};
    fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragModule;
    fragStageInfo.pName = "main";

    return {vertStageInfo, fragStageInfo};
}

void ShaderManager::cleanup() {
    for (auto& [path, module] : cache) {
        vkDestroyShaderModule(device, module, nullptr);
    }
    cache.clear();
}
