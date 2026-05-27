cmake_minimum_required(VERSION 3.24)

# Set project name and language
project(gpuris LANGUAGES CXX)

# Require C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add source files from src/
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")

# Create executable
add_executable(gpuris ${SOURCES})

# Include directories
target_include_directories(gpuris PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/vma
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/vk-bootstrap
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Find Vulkan SDK
find_package(Vulkan REQUIRED)
if(NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan SDK not found.")
endif()

# Specify library paths
set(LIBS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs)

#glfw
add_subdirectory(${LIBS_DIR}/glfw EXCLUDE_FROM_ALL)

#vk-bootstrap
add_subdirectory(${LIBS_DIR}/vk-bootstrap EXCLUDE_FROM_ALL)

# Link libraries
target_link_libraries(gpuris PRIVATE glfw vk-bootstrap Vulkan::Vulkan)

