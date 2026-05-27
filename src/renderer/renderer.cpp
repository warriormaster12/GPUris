#include "renderer.h"
#include "GLFW/glfw3.h"
#include "rhi/vulkan-rhi.h"
#include <iostream>

Renderer::Renderer(RendererBackend backend, GLFWwindow *window /*= nullptr*/) {
  this->window = window;
  switch (backend) {
  case VULKAN:
    rhi = new VulkanRHI(window);
    break;
  default:
    throw std::runtime_error("Only Vulkan backend is supported");
  }
}

void Renderer::update() {
  if (!window) {
    return;
  }

  int width, height;
  glfwGetWindowSize(window, &width, &height);
  rhi->swapchainResize(width, height);

  rhi->prepareFrame();
  rhi->draw();
}

void Renderer::shutdown() {
  rhi->shutdown();
  delete rhi;
}