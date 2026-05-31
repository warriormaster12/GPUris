#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "renderer/renderer.h"

int main(int argc, char *argv[]) {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  GLFWwindow *window = glfwCreateWindow(1280, 720, "GPUris", nullptr, nullptr);

  Renderer renderer = Renderer(RendererBackend::VULKAN, window);

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    renderer.update();
  }

  renderer.shutdown();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}