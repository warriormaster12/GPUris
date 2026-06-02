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

VkShaderStageFlagBits mapShaderStageToVk(ShaderStages stage) {
  switch (stage) {
  case ShaderStages::VERTEX:
    return VK_SHADER_STAGE_VERTEX_BIT;
  case ShaderStages::FRAGMENT:
    return VK_SHADER_STAGE_FRAGMENT_BIT;
  }

  throw std::runtime_error("Invalid shader stage");
}

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

bool VulkanRHI::shouldResizeSwapchain() {
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);
  return swapchain.extent.width != width || swapchain.extent.height != height;
}

std::shared_ptr<RenderPipeline>
VulkanRHI::createRenderPipeline(RenderPipelineInfo &p_info) {

  VkPipelineRenderingCreateInfo rendering_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .viewMask = 0,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &swapchain.format,
      .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
      .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
  };

  std::vector<VkPipelineShaderStageCreateInfo> stages = {};

  for (std::map<ShaderStages, std::vector<char>>::iterator stage =
           p_info.stages.begin();
       stage != p_info.stages.end(); ++stage) {
    VkShaderModuleCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = stage->second.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t *>(stage->second.data())};

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS) {
      std::string errorMessage = "failed to create shader module for stage " +
                                 std::to_string(stage->first);
      throw std::runtime_error(errorMessage);
    }

    VkPipelineShaderStageCreateInfo stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = mapShaderStageToVk(stage->first),
        .module = module,
        .pName = "main"};

    stages.push_back(stage_info);
  }

  VkPipelineVertexInputStateCreateInfo vertexInputState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
  };

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

  VkPipelineViewportStateCreateInfo viewportState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1};

  VkPipelineRasterizationStateCreateInfo rasterizationState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      .depthBiasClamp = VK_FALSE,
      .lineWidth = 1.0f,
  };

  VkPipelineMultisampleStateCreateInfo multisampleState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE};

  VkPipelineColorBlendAttachmentState colorBlendAttachment = {
      .blendEnable = VK_FALSE,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  VkPipelineColorBlendStateCreateInfo colorBlendState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment};

  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
      .pDynamicStates = dynamicStates.data()};

  VkPipelineLayoutCreateInfo layoutInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 0,
      .pSetLayouts = nullptr};

  VkPipelineLayout layout = VK_NULL_HANDLE;
  if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
  }

  VkGraphicsPipelineCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &rendering_info,
      .stageCount = static_cast<uint32_t>(stages.size()),
      .pStages = stages.data(),
      .pVertexInputState = &vertexInputState,
      .pInputAssemblyState = &inputAssemblyState,
      .pViewportState = &viewportState,
      .pRasterizationState = &rasterizationState,
      .pMultisampleState = &multisampleState,
      .pDepthStencilState = nullptr,
      .pColorBlendState = &colorBlendState,
      .pDynamicState = &dynamicState,
      .layout = layout,
      .renderPass = VK_NULL_HANDLE,
  };
  VkPipeline pipeline = VK_NULL_HANDLE;
  if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr,
                                &pipeline) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }

  std::shared_ptr<VulkanRenderPipeline> outRenderPipeline =
      std::make_shared<VulkanRenderPipeline>();
  outRenderPipeline->pipeline = pipeline;
  outRenderPipeline->layout = layout;

  for (const VkPipelineShaderStageCreateInfo &stage : stages) {
    vkDestroyShaderModule(device, stage.module, nullptr);
  }

  return outRenderPipeline;
}

void VulkanRHI::prepareFrame() {
  if (shouldResizeSwapchain()) {
    vkDeviceWaitIdle(device);
    swapchainResize();
  }

  VkResult fenceResult =
      vkWaitForFences(device, 1, &perFrameInfo[currentFrame].inFlightFence,
                      VK_TRUE, UINT64_MAX);
  if (fenceResult != VK_SUCCESS) {
    throw std::runtime_error("failed to wait for fence!");
  }

  VkResult swapchainResult = swapchain.acquireNextImage(
      device, UINT64_MAX, perFrameInfo[currentFrame].presentCompleteSemaphore);

  if (swapchainResult == VK_ERROR_OUT_OF_DATE_KHR) {
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
}

void VulkanRHI::setupViewport(float width, float height, float minDepth,
                              float maxDepth) {
  VkCommandBuffer &commandBuffer = perFrameInfo[currentFrame].commandBuffer;
  VkViewport viewport = {.x = 0.0,
                         .y = 0.0,
                         .width = width,
                         .height = height,
                         .minDepth = minDepth,
                         .maxDepth = maxDepth};
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
}

void VulkanRHI::setupScissor(int32_t x, int32_t y, uint32_t width,
                             uint32_t height) {
  VkCommandBuffer &commandBuffer = perFrameInfo[currentFrame].commandBuffer;

  VkRect2D scissor = {.offset = {.x = x, .y = y},
                      .extent = {.width = width, .height = height}};

  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void VulkanRHI::bindPipeline(std::shared_ptr<Pipeline> p_pipeline) {
  VkCommandBuffer &commandBuffer = perFrameInfo[currentFrame].commandBuffer;
  if (static_cast<VulkanRenderPipeline *>(p_pipeline.get()) != nullptr) {
    VulkanRenderPipeline *pipeline =
        static_cast<VulkanRenderPipeline *>(p_pipeline.get());
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline->pipeline);
  }
}

void VulkanRHI::draw(uint32_t vertexCount, uint32_t instanceCount,
                     uint32_t firstVertex, uint32_t firstInstance) {
  VkCommandBuffer &commandBuffer = perFrameInfo[currentFrame].commandBuffer;
  vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex,
            firstInstance);
}
void VulkanRHI::drawIndexed() {}

void VulkanRHI::submit() {
  VkCommandBuffer &commandBuffer = perFrameInfo[currentFrame].commandBuffer;
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
    break;
  }
  currentFrame = (currentFrame + 1) % perFrameInfo.size();
}

void VulkanRHI::freePipeline(std::shared_ptr<Pipeline> pipeline) {
  VulkanRenderPipeline *vkPipeline =
      static_cast<VulkanRenderPipeline *>(pipeline.get());
  if (vkPipeline) {

    for (const PerFrameData &currentFrame : perFrameInfo) {
      vkWaitForFences(device, 1, &currentFrame.inFlightFence, VK_TRUE,
                      UINT64_MAX);
    }

    if (vkPipeline->pipeline != VK_NULL_HANDLE)
      vkDestroyPipeline(device, vkPipeline->pipeline, nullptr);

    if (vkPipeline->layout != VK_NULL_HANDLE)
      vkDestroyPipelineLayout(device, vkPipeline->layout, nullptr);
  }
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