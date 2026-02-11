#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "rendering/RenderEngine.h"
#include "mesh/GLTFLoader.h"
#include <imgui.h>
#include <iostream>
#include <stdexcept>
#include <cstdlib>

// Test function for glTF loading + full PBR integration
void testGLTFLoader(RenderEngine& engine) {
    std::cout << "\n=== Loading Mech Drone with PBR ===" << std::endl;

    GLTFModel model;
    bool success = GLTFLoader::loadGLTF("main_sponza/NewSponza_Main_glTF_003.gltf", model);
    //bool success = GLTFLoader::loadGLTF("mech_drone/scene.gltf", model);


    if (!success) {
        std::cerr << "[FAILED] Could not load glTF file!" << std::endl;
        std::cout << "=========================\n" << std::endl;
        return;
    }

    std::cout << "\n[SUCCESS] glTF file loaded!" << std::endl;
    std::cout << "  Total Meshes: " << model.meshes.size() << std::endl;
    std::cout << "  Total Materials: " << model.materials.size() << std::endl;
    std::cout << "  Total Textures: " << model.textures.size() << std::endl;

    // Show first material details
    if (!model.materials.empty()) {
        const auto& mat = model.materials[0];
        std::cout << "\nFirst Material '" << mat.name << "':" << std::endl;
        std::cout << "  Base Color Factor: (" << mat.baseColorFactor.r << ", "
                  << mat.baseColorFactor.g << ", " << mat.baseColorFactor.b << ")" << std::endl;
        std::cout << "  Metallic Factor: " << mat.metallicFactor << std::endl;
        std::cout << "  Roughness Factor: " << mat.roughnessFactor << std::endl;
        std::cout << "  BaseColor Texture Index: " << mat.baseColorTextureIndex << std::endl;
        std::cout << "  Normal Texture Index: " << mat.normalTextureIndex << std::endl;
        std::cout << "  MetallicRoughness Texture Index: " << mat.metallicRoughnessTextureIndex << std::endl;
    }

    // Show first texture details
    if (!model.textures.empty()) {
        const auto& tex = model.textures[0];
        std::cout << "\nFirst Texture:" << std::endl;
        std::cout << "  URI: " << tex.uri << std::endl;
        std::cout << "  Full Path: " << tex.fullPath << std::endl;
    }

    std::cout << "\n=== Integrating into Renderer ===" << std::endl;
    std::cout << "Loading " << model.textures.size() << " textures..." << std::endl;

    // Step 1: Load all textures with TextureManager
    std::vector<std::string> texturePaths;
    for (const auto& tex : model.textures) {
        texturePaths.push_back(tex.fullPath);
    }

    std::vector<int> textureIndices = engine.loadTextures(texturePaths);
    std::cout << "Loaded " << textureIndices.size() << " textures into GPU." << std::endl;

    // Step 2: Upload all materials to MaterialManager
    std::cout << "Uploading " << model.materials.size() << " materials..." << std::endl;
    std::vector<int> materialIndices = engine.uploadMaterials(model.materials, textureIndices);
    std::cout << "Uploaded " << materialIndices.size() << " materials to GPU." << std::endl;

    // Step 3: Add all meshes to the scene
    std::cout << "Adding " << model.meshes.size() << " meshes to scene..." << std::endl;
    for (size_t i = 0; i < model.meshes.size(); i++) {
        const auto& gltfMesh = model.meshes[i];

        // Convert and upload mesh to GPU
        SceneObject obj = engine.createSceneObjectFromGLTF(gltfMesh, materialIndices);
        obj.name = gltfMesh.name;

        engine.addSceneObject(std::move(obj));
    }
    std::cout << "Added " << model.meshes.size() << " meshes to scene." << std::endl;

    // Step 4: Switch to PBR shader
    std::cout << "Activating PBR shader..." << std::endl;
    engine.setPBRShader();

    std::cout << "\n[SUCCESS] Sponza loaded with PBR rendering!" << std::endl;
    std::cout << "=========================\n" << std::endl;
}

// Global variables for window position (needed for restore after minimize)
static int g_windowPosX = 0;
static int g_windowPosY = 0;

// Mouse tracking for camera controls
static double g_lastMouseX = 0.0;
static double g_lastMouseY = 0.0;
static bool g_firstMouse = true;
static bool g_altPressed = false;

// Framebuffer resize callback
void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto engine = reinterpret_cast<RenderEngine*>(glfwGetWindowUserPointer(window));
    if (engine) {
        engine->setFramebufferResized(true);
    }
}

// Window iconify callback (minimize/restore)
void windowIconifyCallback(GLFWwindow* window, int iconified) {
    if (!iconified) {
        // Window was restored from minimized state
        // Re-apply position to ensure it's on the correct monitor
        glfwSetWindowPos(window, g_windowPosX, g_windowPosY);
    }
}

// Mouse button callback (detect which mode for camera)
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (action == GLFW_PRESS) {
        // Reset first mouse flag when button is pressed
        g_firstMouse = true;
    }
}

// Mouse cursor position callback (for camera movement)
void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    // Don't process camera movement if ImGui wants the mouse input
    if (ImGui::GetIO().WantCaptureMouse) return;

    auto engine = reinterpret_cast<RenderEngine*>(glfwGetWindowUserPointer(window));
    if (!engine) return;

    if (g_firstMouse) {
        g_lastMouseX = xpos;
        g_lastMouseY = ypos;
        g_firstMouse = false;
        return;
    }

    float xoffset = static_cast<float>(xpos - g_lastMouseX);
    float yoffset = static_cast<float>(g_lastMouseY - ypos); // Reversed: y increases downward
    g_lastMouseX = xpos;
    g_lastMouseY = ypos;

    // Determine camera mode based on mouse buttons and modifiers
    g_altPressed = (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
                    glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS);

    CameraMode mode = CameraMode::NONE;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        if (g_altPressed) {
            mode = CameraMode::ZOOM_DOLLY; // Alt + Right = Zoom
        } else {
            mode = CameraMode::FLY; // Right = Free look
        }
    }
    else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
        mode = CameraMode::PAN; // Middle = Pan
    }
    else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && g_altPressed) {
        mode = CameraMode::ORBIT; // Alt + Left = Orbit
    }

    if (mode != CameraMode::NONE) {
        engine->getCamera().processMouseMovement(xoffset, yoffset, mode);
    }
}

// Mouse scroll callback (for zoom)
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    // Don't process camera zoom if ImGui wants the mouse input
    if (ImGui::GetIO().WantCaptureMouse) return;

    auto engine = reinterpret_cast<RenderEngine*>(glfwGetWindowUserPointer(window));
    if (!engine) return;

    engine->getCamera().processMouseScroll(static_cast<float>(yoffset));
}

// Keyboard callback (for gizmo mode switching)
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    // Don't process scene shortcuts if ImGui wants the keyboard input
    if (ImGui::GetIO().WantCaptureKeyboard) return;

    auto engine = reinterpret_cast<RenderEngine*>(glfwGetWindowUserPointer(window));
    if (!engine) return;

    // Only respond to key press (not release or repeat)
    if (action != GLFW_PRESS) return;

    // Gizmo mode switching (W/E/R keys)
    switch (key) {
        case GLFW_KEY_W:
            engine->setGizmoMode(GizmoMode::Translate);
            std::cout << "Gizmo Mode: Translate" << std::endl;
            break;
        case GLFW_KEY_E:
            engine->setGizmoMode(GizmoMode::Rotate);
            std::cout << "Gizmo Mode: Rotate" << std::endl;
            break;
        case GLFW_KEY_R:
            engine->setGizmoMode(GizmoMode::Scale);
            std::cout << "Gizmo Mode: Scale" << std::endl;
            break;
        case GLFW_KEY_DELETE:
        case GLFW_KEY_BACKSPACE:
            // Delete selected object
            if (engine->getSelectedObjectIndex() >= 0) {
                engine->deleteObject(static_cast<size_t>(engine->getSelectedObjectIndex()));
                std::cout << "Deleted selected object" << std::endl;
            }
            break;
    }
}

// Drag & Drop callback for loading models/textures at runtime
void dropCallback(GLFWwindow* window, int count, const char** paths) {
    auto engine = reinterpret_cast<RenderEngine*>(glfwGetWindowUserPointer(window));
    if (!engine) return;

    for (int i = 0; i < count; i++) {
        std::string filepath(paths[i]);
        std::cout << "Dropped file: " << filepath << std::endl;

        // Detect file extension
        std::string ext;
        size_t dotPos = filepath.find_last_of('.');
        if (dotPos != std::string::npos) {
            ext = filepath.substr(dotPos);
            // Convert to lowercase for comparison
            for (auto& c : ext) c = std::tolower(c);
        }

        // Route to appropriate loader
        if (ext == ".obj" || ext == ".txt") {
            engine->loadMesh(filepath);
        } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") {
            engine->loadTexture(filepath);
        } else {
            std::cout << "Unsupported file type: " << ext << std::endl;
        }
    }
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);  // Enable window resizing
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);  // Window Look
    // Note: NOT using GLFW_MAXIMIZED here - we maximize AFTER positioning

    // Query available monitors
    int monitorCount;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

    // Select target monitor (prefer second monitor if available)
    GLFWmonitor* targetMonitor = nullptr;
    int windowWidth, windowHeight, windowPosX, windowPosY;

    if (monitorCount >= 2) {
        // Use second monitor
        targetMonitor = monitors[1];
        std::cout << "Using second monitor for windowed fullscreen" << std::endl;
    } else {
        // Fallback to primary monitor
        targetMonitor = glfwGetPrimaryMonitor();
        std::cout << "Only one monitor detected, using primary monitor" << std::endl;
    }

    // Get monitor work area (for positioning)
    glfwGetMonitorWorkarea(targetMonitor, &windowPosX, &windowPosY, &windowWidth, &windowHeight);

    // Store position in global variables for restore callback
    g_windowPosX = windowPosX;
    g_windowPosY = windowPosY;

    std::cout << "Monitor work area: " << windowWidth << "x" << windowHeight << std::endl;
    std::cout << "Work area position: " << windowPosX << ", " << windowPosY << std::endl;

    // Create window with initial size
    GLFWwindow* window = glfwCreateWindow(800, 600, "Mini Renderer", nullptr, nullptr);

    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // IMPORTANT: Position window on target monitor BEFORE maximizing
    // This ensures maximization happens on the correct monitor
    glfwSetWindowPos(window, windowPosX, windowPosY);

    // Small delay to ensure position is applied before maximizing
    glfwPollEvents();

    // Now maximize - GLFW will maximize on the monitor where the window is currently positioned
    glfwMaximizeWindow(window);

    RenderEngine engine;

    // Register callbacks
    glfwSetWindowUserPointer(window, &engine);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetWindowIconifyCallback(window, windowIconifyCallback);
    glfwSetDropCallback(window, dropCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetKeyCallback(window, keyCallback);

    try {
        engine.init(window);  // Start empty - use Drag & Drop to load meshes
        std::cout << "RenderEngine initialized (empty scene - drag & drop .obj files to load)." << std::endl;

        // TEST: glTF loader - Load Sponza with PBR
        testGLTFLoader(engine);

        // Load camera state from file (if exists)
        engine.getCamera().loadState("camera_state.txt");

        // Delta time tracking for smooth camera movement
        float deltaTime = 0.0f;
        float lastFrame = 0.0f;

        int frameCount = 0;
        while (!glfwWindowShouldClose(window)) {
            // Calculate delta time
            float currentFrame = static_cast<float>(glfwGetTime());
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            glfwPollEvents();

            // Process camera keyboard input (WASD + Q/E with Right Mouse Button)
            engine.getCamera().processInput(window, deltaTime);

            if (frameCount % 30 == 0) {
                engine.checkAndReloadMesh();
            }

            engine.drawFrame();
            frameCount++;
        }

        // Save camera state before cleanup
        engine.getCamera().saveState("camera_state.txt");

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
