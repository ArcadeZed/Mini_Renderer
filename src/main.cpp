#include "Renderer.h"
#include <iostream>

int main() {
    Renderer renderer;

    try {
        renderer.init();
        std::cout << "Renderer initialized." << std::endl;

        // Main loop placeholder
        for (int i = 0; i < 100; i++) {
            renderer.drawFrame();
        }

        renderer.cleanup();
        std::cout << "Renderer cleaned up." << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}