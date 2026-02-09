#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "rendering/RenderEngine.h"
#include <iostream>
#include <stdexcept>
#include <cstdlib>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Mini Renderer", nullptr, nullptr);

    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return EXIT_FAILURE;
    }

    RenderEngine engine;

    try {
        engine.init(window);
        std::cout << "RenderEngine initialized." << std::endl;

        int frameCount = 0;
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            if (frameCount % 30 == 0) {
                engine.checkAndReloadMesh();
            }

            engine.drawFrame();
            frameCount++;
        }

        engine.cleanup();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
