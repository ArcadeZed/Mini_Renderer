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

# Shader automatisch kompilieren (optional, falls glslc verfügbar)
file(GLOB SHADER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag")

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
        src/Renderer.cpp
        src/Renderer.h
        src/vulkan/VulkanContext.cpp
        src/vulkan/VulkanContext.h
)

add_dependencies(MiniRenderer Shaders)

target_link_libraries(MiniRenderer
        Vulkan::Vulkan
        glfw
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