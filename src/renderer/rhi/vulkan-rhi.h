#include "rhi.h"
#include "vk_mem_alloc.h"
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

class GLFWwindow;

class VulkanRHI : public RHI {
public:
  VulkanRHI();
  VulkanRHI(GLFWwindow *p_window);
  std::shared_ptr<RenderPipeline>
  createRenderPipeline(RenderPipelineInfo &p_info) override;
  void prepareFrame() override;
  virtual void setupViewport(float width, float height, float minDepth,
                             float maxDepth) override;
  virtual void setupScissor(int32_t x, int32_t y, uint32_t width,
                            uint32_t height) override;
  virtual void bindPipeline(std::shared_ptr<Pipeline> p_pipeline) override;
  virtual void draw(uint32_t vertexCount, uint32_t instanceCount,
                    uint32_t firstVertex, uint32_t firstInstance) override;
  virtual void drawIndexed() override;
  virtual void submit() override;
  virtual void freePipeline(std::shared_ptr<Pipeline> pipeline) override;
  virtual void shutdown() override;

private:
  void swapchainResize();
  bool shouldResizeSwapchain();
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

  struct VulkanRenderPipeline : RenderPipeline {
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
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