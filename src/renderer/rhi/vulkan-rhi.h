#include "rhi.h"
#include "vk_mem_alloc.h"
#include <vector>
#include <vulkan/vulkan.h>

class GLFWwindow;

class VulkanRHI : public RHI {
public:
  VulkanRHI();
  VulkanRHI(GLFWwindow *p_window);
  void prepareFrame() override;
  void draw() override;
  void shutdown() override;

private:
  void swapchainResize();
  void transitionImageLayout(VkImage inputImage, VkImageLayout oldLayout,
                             VkImageLayout newLayout,
                             VkAccessFlags2 srcAccessMask,
                             VkAccessFlags2 dstAccessMask,
                             VkPipelineStageFlags2 srcStageMask,
                             VkPipelineStageFlags2 dstStageMask);

  struct Swapchain {
    VkExtent2D extent;
    VkFormat format;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    uint32_t acquiredImageIndex;

    VkResult acquireNextImage(VkDevice &device, uint64_t timeout,
                              VkSemaphore semaphore) {
      return vkAcquireNextImageKHR(device, swapchain, timeout, semaphore,
                                   nullptr, &acquiredImageIndex);
    }

    void destroy(VkDevice &device) {
      for (int i = 0; i < views.size(); ++i) {
        vkDestroyImageView(device, views[i], nullptr);
      }

      if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
      }
    }
  };

  struct PerFrameData {
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkSemaphore presentCompleteSemaphore;
    VkFence inFlightFence;
  };

  GLFWwindow *window = nullptr;
  VkInstance instance = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkQueue graphicsQueue = VK_NULL_HANDLE;
  uint32_t graphicsQueueFamilyIndex = 0;
  VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  Swapchain swapchain;
  VmaAllocator allocator;
  std::vector<PerFrameData> perFrameInfo = {};
  std::vector<VkSemaphore> renderFinishedSemaphores = {};
  std::pair<VkResult, uint32_t> swapchainImageResult;

  int currentFrame = 0;
};
