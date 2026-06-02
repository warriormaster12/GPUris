#include "renderer.h"
#include "GLFW/glfw3.h"
#include "io/file.h"
#include "rhi/vulkan-rhi.h"
#include <iostream>
#include <memory>
std::shared_ptr<RenderPipeline> pipeline;

Renderer::Renderer(RendererBackend backend, GLFWwindow *window /*= nullptr*/) {
  this->window = window;
  switch (backend) {
  case VULKAN:
    rhi = new VulkanRHI(window);
    break;
  default:
    throw std::runtime_error("Only Vulkan backend is supported");
  }

  RenderPipelineInfo info = {
      .stages = {
          {ShaderStages::VERTEX, File::read("shaders/main.vert.spv")},
          {ShaderStages::FRAGMENT, File::read("shaders/main.frag.spv")}}};
  pipeline = rhi->createRenderPipeline(info);
}

void Renderer::update() {
  if (!window) {
    return;
  }

  int windowWidth, windowHeight;
  glfwGetFramebufferSize(window, &windowWidth, &windowHeight);

  rhi->prepareFrame();
  rhi->setupViewport(windowWidth, windowHeight, 0.0, 1.0);
  rhi->setupScissor(0, 0, windowWidth, windowHeight);
  rhi->bindPipeline(pipeline);
  rhi->draw(3, 1, 0, 0);
  rhi->submit();
}

void Renderer::shutdown() {
  rhi->freePipeline(pipeline);
  rhi->shutdown();
  delete rhi;
}