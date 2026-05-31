#include "vulkan-rhi.h"
#include <VkBootstrap.h>
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <iostream>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VulkanRHI::VulkanRHI(GLFWwindow *p_window) {
  window = p_window;
  vkb::InstanceBuilder builder;
  vkb::Result<vkb::Instance> inst_ret = builder.set_app_name("Vulkan App")
                                            .request_validation_layers()
                                            .use_default_debug_messenger()
                                            .require_api_version(1, 4)
                                            .build();
  if (!inst_ret) {
  }
  vkb::Instance vkb_inst = inst_ret.value();
  instance = vkb_inst.instance;
  debugMessenger = vkb_inst.debug_messenger;

  if (glfwCreateWindowSurface(instance, window, nullptr, &surface) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create window surface");
  }

  VkPhysicalDeviceVulkan13Features feats13 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .synchronization2 = VK_TRUE,
      .dynamicRendering = VK_TRUE};

  VkPhysicalDeviceVulkan14Features feats14 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
  };

  vkb::PhysicalDeviceSelector selector{vkb_inst};
  vkb::Result<vkb::PhysicalDevice> phys_ret =
      selector.set_surface(surface)
          .set_minimum_version(1, 4)
          .set_required_features_13(feats13)
          .set_required_features_14(feats14)
          .prefer_gpu_device_type()
          .select();
  if (!phys_ret) {
    throw std::runtime_error("Failed to create suitable physical device");
  } else {
    std::cout << "Selected device: " << phys_ret->properties.deviceName
              << std::endl;
  }

  physicalDevice = phys_ret->physical_device;

  vkb::DeviceBuilder device_builder{phys_ret.value()};
  vkb::Result<vkb::Device> dev_ret = device_builder.build();
  if (!dev_ret) {
    throw std::runtime_error("Failed to create suitable logical device");
  }

  vkb::Device vkb_device = dev_ret.value();
  device = vkb_device.device;

  vkb::Result<std::pair<VkQueue, uint32_t>> graphics_queue_ret =
      vkb_device.get_queue_and_index(vkb::QueueType::graphics);
  if (!graphics_queue_ret) {
    throw std::runtime_error("Failed to create graphics queue");
  }
  graphicsQueue = graphics_queue_ret.value().first;
  graphicsQueueFamilyIndex = graphics_queue_ret.value().second;

  VmaAllocatorCreateInfo allocatorCreateInfo = {
      .flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT,
      .vulkanApiVersion = VK_API_VERSION_1_4,
  };

  allocatorCreateInfo.instance = instance;
  allocatorCreateInfo.physicalDevice = physicalDevice;
  allocatorCreateInfo.device = device;

  vmaCreateAllocator(&allocatorCreateInfo, &allocator);

  std::cout << "Vulkan initialized" << std::endl;

  swapchainResize();

  perFrameInfo.resize(2);

  VkCommandPoolCreateInfo commandPoolInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = graphicsQueueFamilyIndex};

  for (int i = 0; i < perFrameInfo.size(); ++i) {
    if (vkCreateCommandPool(vkb_device, &commandPoolInfo, nullptr,
                            &perFrameInfo[i].commandPool) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create command pool");
    }

    VkCommandBufferAllocateInfo commandBufferAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = perFrameInfo[i].commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1};

    if (vkAllocateCommandBuffers(device, &commandBufferAllocInfo,
                                 &perFrameInfo[i].commandBuffer) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed to allocate command buffer");
    }
  }

  renderFinishedSemaphores.resize(swapchain.views.size());
  for (int i = 0; i < renderFinishedSemaphores.size(); i++) {
    VkSemaphoreCreateInfo info = {.sType =
                                      VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    if (vkCreateSemaphore(device, &info, nullptr,
                          &renderFinishedSemaphores[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create renderFinishedSemaphore");
    }
  }

  for (int i = 0; i < perFrameInfo.size(); i++) {
    VkSemaphoreCreateInfo semInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    if (vkCreateSemaphore(device, &semInfo, nullptr,
                          &perFrameInfo[i].presentCompleteSemaphore) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed to create presentCompleteSemaphore");
    }

    VkFenceCreateInfo fenceInfo = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                   .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    if (vkCreateFence(device, &fenceInfo, nullptr,
                      &perFrameInfo[i].inFlightFence) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create inFlightFence");
    }
  }
}

void VulkanRHI::swapchainResize() {
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);
  if (swapchain.extent.width == width && swapchain.extent.height == height)
    return;

  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(window, &width, &height);
    glfwWaitEvents();
  }

  swapchain.destroy(device);

  vkb::SwapchainBuilder swapchain_builder{physicalDevice, device, surface};
  vkb::Swapchain vkb_swapchain =
      swapchain_builder.use_default_format_selection()
          .set_desired_extent(width, height)
          .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
          .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
          .build()
          .value();

  swapchain = {.extent = vkb_swapchain.extent,
               .format = vkb_swapchain.image_format,
               .swapchain = vkb_swapchain.swapchain,
               .images = vkb_swapchain.get_images().value(),
               .views = vkb_swapchain.get_image_views().value()};
}
void VulkanRHI::prepareFrame() {
  VkResult fenceResult =
      vkWaitForFences(device, 1, &perFrameInfo[currentFrame].inFlightFence,
                      VK_TRUE, UINT64_MAX);
  if (fenceResult != VK_SUCCESS) {
    throw std::runtime_error("failed to wait for fence!");
  }

  VkResult swapchainResult = swapchain.acquireNextImage(
      device, UINT64_MAX, perFrameInfo[currentFrame].presentCompleteSemaphore);

  if (swapchainResult == VK_ERROR_OUT_OF_DATE_KHR) {
    swapchainResize();
    return;
  }

  vkResetFences(device, 1, &perFrameInfo[currentFrame].inFlightFence);

  VkCommandBuffer &commandBuffer = perFrameInfo[currentFrame].commandBuffer;

  vkResetCommandBuffer(commandBuffer, 0);

  VkCommandBufferBeginInfo beginInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  // Before starting rendering, transition the swapchain image to
  // vk::ImageLayout::eColorAttachmentOptimal
  transitionImageLayout(
      swapchain.images[swapchain.acquiredImageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      {}, // srcAccessMask (no need to wait for previous operations)
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,          // dstAccessMask
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStage
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT  // dstStage
  );

  VkRenderingAttachmentInfo colorAttachmentInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = swapchain.views[swapchain.acquiredImageIndex],
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = {{0.0f, 1.0f, 0.0f, 1.0f}}};

  VkRenderingInfo renderingInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = {.offset = {0, 0}, .extent = swapchain.extent},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachmentInfo};

  vkCmdBeginRendering(commandBuffer, &renderingInfo);
  vkCmdEndRendering(commandBuffer);

  // After rendering, transition the swapchain image to
  // vk::ImageLayout::ePresentSrcKHR
  transitionImageLayout(
      swapchain.images[swapchain.acquiredImageIndex],
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,          // srcAccessMask
      {},                                              // dstAccessMask
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStage
      VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT           // dstStage
  );
  vkEndCommandBuffer(commandBuffer);
}
void VulkanRHI::draw() {

  const VkPipelineStageFlags waitDestinationStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  const VkSubmitInfo submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &perFrameInfo[currentFrame].presentCompleteSemaphore,
      .pWaitDstStageMask = &waitDestinationStageMask,
      .commandBufferCount = 1,
      .pCommandBuffers = &perFrameInfo[currentFrame].commandBuffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores =
          &renderFinishedSemaphores[swapchain.acquiredImageIndex]};

  vkQueueSubmit(graphicsQueue, 1, &submitInfo,
                perFrameInfo[currentFrame].inFlightFence);

  const VkPresentInfoKHR presentInfoKHR = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores =
          &renderFinishedSemaphores[swapchain.acquiredImageIndex],
      .swapchainCount = 1,
      .pSwapchains = &swapchain.swapchain,
      .pImageIndices = &swapchain.acquiredImageIndex};

  VkResult result = vkQueuePresentKHR(graphicsQueue, &presentInfoKHR);
  switch (result) {
  case VK_SUCCESS:
    break;
  case VK_SUBOPTIMAL_KHR:
  case VK_ERROR_OUT_OF_DATE_KHR:
    swapchainResize();
    break;
  default:
    break; // an unexpected result is returned!
  }
  currentFrame = (currentFrame + 1) % perFrameInfo.size();
}

void VulkanRHI::shutdown() {
  vkDeviceWaitIdle(device);

  for (int i = 0; i < renderFinishedSemaphores.size(); ++i) {
    vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
  }

  for (PerFrameData &currentFrame : perFrameInfo) {
    vkDestroyCommandPool(device, currentFrame.commandPool, nullptr);
    vkDestroySemaphore(device, currentFrame.presentCompleteSemaphore, nullptr);
    vkDestroyFence(device, currentFrame.inFlightFence, nullptr);
  }

  swapchain.destroy(device);
  vmaDestroyAllocator(allocator);
  vkDestroyDevice(device, nullptr);
  vkDestroySurfaceKHR(instance, surface, nullptr);
  vkb::destroy_debug_utils_messenger(instance, debugMessenger);
  vkDestroyInstance(instance, nullptr);
}

void VulkanRHI::transitionImageLayout(
    VkImage inputImage, VkImageLayout oldLayout, VkImageLayout newLayout,
    VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask,
    VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask) {
  VkImageMemoryBarrier2 barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .srcStageMask = srcStageMask,
      .srcAccessMask = srcAccessMask,
      .dstStageMask = dstStageMask,
      .dstAccessMask = dstAccessMask,
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = inputImage,
      .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};
  VkDependencyInfo dependencyInfo = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                     .dependencyFlags = 0,
                                     .imageMemoryBarrierCount = 1,
                                     .pImageMemoryBarriers = &barrier};
  vkCmdPipelineBarrier2(perFrameInfo[currentFrame].commandBuffer,
                        &dependencyInfo);
}