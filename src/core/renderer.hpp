#pragma once

#include "core/core.hpp"
#include <vector>

namespace luster {

class Renderer {
public:
  Renderer();
  ~Renderer();

  void init(SDL_Window *window);
  bool drawFrame(SDL_Window *window);
  void recreateSwapchain(SDL_Window *window);
  void cleanup();

private:
  struct VulkanState {
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice gpu = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t gfxQueueFamily = 0;
    uint32_t presentQueueFamily = 0;
    VkQueue gfxQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR swapColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkExtent2D swapExtent{};
    std::vector<VkImage> swapImages;
    std::vector<VkImageView> swapImageViews;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers;

    VkCommandPool cmdPool = VK_NULL_HANDLE;
    VkCommandBuffer cmdBuf = VK_NULL_HANDLE;

    VkSemaphore semImageAvailable = VK_NULL_HANDLE;
    VkSemaphore semRenderFinished = VK_NULL_HANDLE;
    VkFence inFlight = VK_NULL_HANDLE;

    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
  } vk_{};

  void createInstance(SDL_Window *window);
  void pickDevice();
  void createDevice();
  void createSwapchainAndViews(SDL_Window *window);
  void createRenderPass();
  void createPipeline();
  void createFramebuffers();
  void createCommandsAndSync();
  void cleanupSwapchain();
  void recordAndSubmitFrame(uint32_t imageIndex);
};

} // namespace luster
