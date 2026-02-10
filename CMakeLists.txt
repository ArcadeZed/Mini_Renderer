cmake_minimum_required(VERSION 3.15)
project(MiniRenderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option für Vulkan Validation Layers
option(ENABLE_VALIDATION_LAYERS "Enable Vulkan Validation Layers (disable if OBS/Overwolf conflict)" OFF)

# Vulkan SDK
find_package(Vulkan REQUIRED)

# GLFW (als Submodul oder externes Verzeichnis)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/glfw)

# ImGui static library
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
add_library(imgui STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
)
target_include_directories(imgui PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)
target_link_libraries(imgui PUBLIC Vulkan::Vulkan glfw)

# Shader automatisch kompilieren
file(GLOB SHADER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.geom")

# Create shader output directory
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders")

set(COMPILED_SHADERS "")
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(FILENAME ${SHADER} NAME)
    set(SPIRV "${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILENAME}.spv")
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SHADER} -o ${SPIRV}
            DEPENDS ${SHADER}
            COMMENT "Compiling shader ${FILENAME}"
    )
    list(APPEND COMPILED_SHADERS ${SPIRV})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${COMPILED_SHADERS})

# Projekt
add_executable(MiniRenderer
        src/main.cpp
        src/rendering/RenderEngine.cpp
        src/rendering/RenderEngine.h
        src/rendering/ForwardPass.cpp
        src/rendering/ForwardPass.h
        src/debug/DebugRenderer.cpp
        src/debug/DebugRenderer.h
        src/debug/ImGuiOverlay.cpp
        src/debug/ImGuiOverlay.h
        src/core/VulkanContext.cpp
        src/core/VulkanContext.h
        src/core/VulkanSwapchain.cpp
        src/core/VulkanSwapchain.h
        src/core/VulkanCommand.cpp
        src/core/VulkanCommand.h
        src/core/VulkanResource.cpp
        src/core/VulkanResource.h
        src/core/ShaderManager.cpp
        src/core/ShaderManager.h
        src/core/PipelineBuilder.cpp
        src/core/PipelineBuilder.h
        src/core/DescriptorManager.cpp
        src/core/DescriptorManager.h
        src/mesh/Mesh.cpp
        src/mesh/Mesh.h
        src/mesh/MeshLoader.cpp
        src/mesh/MeshLoader.h
        src/scene/Camera.cpp
        src/scene/Camera.h
        src/scene/Transform.h
        src/scene/SceneObject.h
        src/scene/Scene.h
)

add_dependencies(MiniRenderer Shaders)

target_link_libraries(MiniRenderer
        Vulkan::Vulkan
        glfw
        imgui
)

target_include_directories(MiniRenderer PRIVATE
        ${Vulkan_INCLUDE_DIRS}
        external/glfw/include
        src
)

target_compile_definitions(MiniRenderer PRIVATE
        SHADER_DIR=\"${CMAKE_CURRENT_BINARY_DIR}/shaders\"
)

# Validation Layers aktivieren, falls Option gesetzt
if(ENABLE_VALIDATION_LAYERS)
    target_compile_definitions(MiniRenderer PRIVATE FORCE_ENABLE_VALIDATION_LAYERS)
    message(STATUS "Vulkan Validation Layers: ENABLED")
else()
    message(STATUS "Vulkan Validation Layers: DISABLED (use -DENABLE_VALIDATION_LAYERS=ON to enable)")
endif()
