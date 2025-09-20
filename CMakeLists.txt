cmake_minimum_required(VERSION 3.15)
project(MiniRenderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Vulkan SDK
find_package(Vulkan REQUIRED)

# GLFW für Fenster/Surface (empfohlen)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/glfw)

include_directories(${Vulkan_INCLUDE_DIRS} src external/glfw/include)

add_executable(MiniRenderer
        src/main.cpp
        src/Renderer.cpp
        src/Renderer.h
)

target_link_libraries(MiniRenderer Vulkan::Vulkan glfw)