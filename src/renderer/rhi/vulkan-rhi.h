#include "rhi.h"
#include "vk_mem_alloc.h"
#include <vector>
#include <vulkan/vulkan.h>

class GLFWwindow;

struct Swapchain {
  VkExtent2D extent;
  VkFormat format;
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  std::vector<VkImageView> views;
  void destroy(VkDevice &device) {
    for (int i = 0; i < views.size(); ++i) {
      vkDestroyImageView(device, views[i], nullptr);
    }

    if (swapchain != VK_NULL_HANDLE) {
      vkDestroySwapchainKHR(device, swapchain, nullptr);
    }
  }
};

class VulkanRHI : public RHI {
public:
  VulkanRHI();
  VulkanRHI(GLFWwindow *window);
  void prepareFrame() override;
  void swapchainResize(uint32_t width, uint32_t height) override;
  void draw() override;
  void shutdown() override;

private:
  VkInstance instance = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  Swapchain swapchain;
  VmaAllocator allocator;
};
