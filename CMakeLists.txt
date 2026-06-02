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

find_program(GLSL_VALIDATOR glslangValidator HINTS 
  ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} 
  /usr/bin 
  /usr/local/bin 
  ${VULKAN_SDK_PATH}/Bin
  ${VULKAN_SDK_PATH}/Bin32
  $ENV{VULKAN_SDK}/Bin/ 
  $ENV{VULKAN_SDK}/Bin32/
)
 
# get all .vert and .frag files in shaders directory
file(GLOB_RECURSE GLSL_SOURCE_FILES
  "${PROJECT_SOURCE_DIR}/src/shaders/*.frag"
  "${PROJECT_SOURCE_DIR}/src/shaders/*.vert"
)

file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/shaders")

set(SPIRV_BINARY_FILES)
 
foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)
 
add_custom_target(
    Shaders
    DEPENDS ${SPIRV_BINARY_FILES}
)