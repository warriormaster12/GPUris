#include "vulkan-rhi.h"
#include <VkBootstrap.h>
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include <iostream>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VulkanRHI::VulkanRHI(GLFWwindow *window) {

  vkb::InstanceBuilder builder;
  vkb::Result<vkb::Instance> inst_ret = builder.set_app_name("Vulkan App")
                                            .request_validation_layers()
                                            .use_default_debug_messenger()
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

  vkb::PhysicalDeviceSelector selector{vkb_inst};
  vkb::Result<vkb::PhysicalDevice> phys_ret = selector.set_surface(surface)
                                                  .set_minimum_version(1, 4)
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

  vkb::Result<VkQueue> graphics_queue_ret =
      vkb_device.get_queue(vkb::QueueType::graphics);
  if (!graphics_queue_ret) {
    throw std::runtime_error("Failed to create graphics queue");
  }
  VkQueue graphics_queue = graphics_queue_ret.value();

  std::cout << "Vulkan initialized" << std::endl;
}

void VulkanRHI::swapchainResize(uint32_t width, uint32_t height) {

  if (swapchain.extent.width == width && swapchain.extent.height == height)
    return;

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
               .views = vkb_swapchain.get_image_views().value()};
}
void VulkanRHI::prepareFrame() {}
void VulkanRHI::draw() {}

void VulkanRHI::shutdown() {
  swapchain.destroy(device);
  vkDestroyDevice(device, nullptr);
  vkDestroySurfaceKHR(instance, surface, nullptr);
  vkb::destroy_debug_utils_messenger(instance, debugMessenger);
  vkDestroyInstance(instance, nullptr);
}