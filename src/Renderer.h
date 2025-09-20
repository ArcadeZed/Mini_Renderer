#pragma once
#include <vulkan/vulkan.h>

class Renderer {
public:
    Renderer();
    ~Renderer();

    void init();
    void drawFrame();
    void cleanup();

private:
    void initVulkan();
    void createSwapchain();
    void createGraphicsPipeline();

    VkInstance instance;
    VkDevice device;
    VkSwapchainKHR swapchain;
    VkPipeline graphicsPipeline;
};