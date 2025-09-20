#include "Renderer.h"
#include <stdexcept>
#include <iostream>

Renderer::Renderer() {}
Renderer::~Renderer() {}

void Renderer::init() {
    initVulkan();
}

void Renderer::initVulkan() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "MiniRenderer";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create Vulkan instance!");
    }

    std::cout << "Vulkan instance created successfully!" << std::endl;
}

void Renderer::drawFrame() {
    // Platzhalter: später Swapchain + CommandBuffers
}

void Renderer::cleanup() {
    vkDestroyInstance(instance, nullptr);
}
