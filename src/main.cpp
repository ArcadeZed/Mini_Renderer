#define GLFW_INCLUDE_VULKAN // Stellt sicher, dass GLFW die Vulkan-Header integriert
#include <GLFW/glfw3.h>
#include "Renderer.h"
#include <iostream>
#include <stdexcept>
#include <cstdlib> // Für EXIT_SUCCESS, EXIT_FAILURE

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

int main() {
    // GLFW initialisieren
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Sagen, dass wir kein OpenGL wollen
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // Fürs Erste nicht resizable

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Hello Triangle", nullptr, nullptr);

    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return EXIT_FAILURE;
    }

    Renderer renderer;

    try {
        renderer.init(window); // Fenster an Renderer übergeben
        std::cout << "Renderer initialized." << std::endl;

        // Main loop
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents(); // GLFW-Events verarbeiten (z.B. Fensterschließen)
            renderer.drawFrame();
        }

        renderer.cleanup();
        std::cout << "Renderer cleaned up." << std::endl;
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