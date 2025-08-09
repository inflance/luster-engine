#include "core/renderer.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace luster {

// SDL error logging helper
static void log_sdl_error(const char *what) {
  const char *err = SDL_GetError();
  spdlog::error("{}: {}", what, (err && *err) ? err : "unknown");
}

static const char *vk_err(VkResult r) {
  switch (r) {
  case VK_SUCCESS:
    return "VK_SUCCESS";
  case VK_NOT_READY:
    return "VK_NOT_READY";
  case VK_TIMEOUT:
    return "VK_TIMEOUT";
  case VK_EVENT_SET:
    return "VK_EVENT_SET";
  case VK_EVENT_RESET:
    return "VK_EVENT_RESET";
  case VK_INCOMPLETE:
    return "VK_INCOMPLETE";
  case VK_ERROR_OUT_OF_HOST_MEMORY:
    return "VK_ERROR_OUT_OF_HOST_MEMORY";
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
  case VK_ERROR_INITIALIZATION_FAILED:
    return "VK_ERROR_INITIALIZATION_FAILED";
  case VK_ERROR_DEVICE_LOST:
    return "VK_ERROR_DEVICE_LOST";
  case VK_ERROR_MEMORY_MAP_FAILED:
    return "VK_ERROR_MEMORY_MAP_FAILED";
  case VK_ERROR_LAYER_NOT_PRESENT:
    return "VK_ERROR_LAYER_NOT_PRESENT";
  case VK_ERROR_EXTENSION_NOT_PRESENT:
    return "VK_ERROR_EXTENSION_NOT_PRESENT";
  case VK_ERROR_FEATURE_NOT_PRESENT:
    return "VK_ERROR_FEATURE_NOT_PRESENT";
  case VK_ERROR_INCOMPATIBLE_DRIVER:
    return "VK_ERROR_INCOMPATIBLE_DRIVER";
  case VK_ERROR_TOO_MANY_OBJECTS:
    return "VK_ERROR_TOO_MANY_OBJECTS";
  case VK_ERROR_FORMAT_NOT_SUPPORTED:
    return "VK_ERROR_FORMAT_NOT_SUPPORTED";
  case VK_ERROR_SURFACE_LOST_KHR:
    return "VK_ERROR_SURFACE_LOST_KHR";
  case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
    return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
  case VK_SUBOPTIMAL_KHR:
    return "VK_SUBOPTIMAL_KHR";
  case VK_ERROR_OUT_OF_DATE_KHR:
    return "VK_ERROR_OUT_OF_DATE_KHR";
  case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
    return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
  case VK_ERROR_VALIDATION_FAILED_EXT:
    return "VK_ERROR_VALIDATION_FAILED_EXT";
  case VK_ERROR_INVALID_SHADER_NV:
    return "VK_ERROR_INVALID_SHADER_NV";
  default:
    return "VK_UNKNOWN";
  }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageTypes,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void *pUserData) {
  (void)messageTypes;
  (void)pUserData;
  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    spdlog::error("[Vulkan] {}",
                  pCallbackData ? pCallbackData->pMessage : "(null)");
  else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    spdlog::warn("[Vulkan] {}",
                 pCallbackData ? pCallbackData->pMessage : "(null)");
  else
    spdlog::info("[Vulkan] {}",
                 pCallbackData ? pCallbackData->pMessage : "(null)");
  return VK_FALSE;
}

static void fillDebugCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &info) {
  info = {};
  info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
  info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  info.pfnUserCallback = debugCallback;
}

static bool hasInstanceLayer(const char *name) {
  uint32_t count = 0;
  if (vkEnumerateInstanceLayerProperties(&count, nullptr) != VK_SUCCESS)
    return false;
  std::vector<VkLayerProperties> layers(count);
  if (count)
    vkEnumerateInstanceLayerProperties(&count, layers.data());
  for (auto &lp : layers)
    if (std::strcmp(lp.layerName, name) == 0)
      return true;
  return false;
}

static bool hasInstanceExtension(const char *name) {
  uint32_t count = 0;
  if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) !=
      VK_SUCCESS)
    return false;
  std::vector<VkExtensionProperties> exts(count);
  if (count)
    vkEnumerateInstanceExtensionProperties(nullptr, &count, exts.data());
  for (auto &ep : exts)
    if (std::strcmp(ep.extensionName, name) == 0)
      return true;
  return false;
}

static VkResult setupDebugMessenger(VkInstance instance,
                                    VkDebugUtilsMessengerEXT *out) {
  if (!out)
    return VK_ERROR_INITIALIZATION_FAILED;
  auto fpCreate = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
  if (!fpCreate)
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  VkDebugUtilsMessengerCreateInfoEXT info{};
  fillDebugCreateInfo(info);
  return fpCreate(instance, &info, nullptr, out);
}

static void destroyDebugMessenger(VkInstance instance,
                                  VkDebugUtilsMessengerEXT msgr) {
  if (!msgr)
    return;
  auto fpDestroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
  if (fpDestroy)
    fpDestroy(instance, msgr, nullptr);
}

struct QueueFamilies {
  std::optional<uint32_t> graphics;
  std::optional<uint32_t> present;
  bool complete() const { return graphics.has_value() && present.has_value(); }
};

struct SwapchainSupport {
  VkSurfaceCapabilitiesKHR caps{};
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

static std::vector<char> readFileBinary(const std::string &path) {
  std::ifstream f(path, std::ios::ate | std::ios::binary);
  if (!f)
    throw std::runtime_error("Failed to open file: " + path);
  const size_t size = static_cast<size_t>(f.tellg());
  std::vector<char> buffer(size);
  f.seekg(0);
  if (!f.read(buffer.data(), buffer.size())) {
    throw std::runtime_error("Failed to read file: " + path);
  }
  return buffer;
}

static QueueFamilies findQueueFamilies(VkPhysicalDevice gpu,
                                       VkSurfaceKHR surface) {
  QueueFamilies out{};
  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, nullptr);
  std::vector<VkQueueFamilyProperties> props(count);
  vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, props.data());
  for (uint32_t i = 0; i < count; ++i) {
    if (!out.graphics && (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
      out.graphics = i;
    }
    if (!out.present) {
      VkBool32 presentSupport = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &presentSupport);
      if (presentSupport)
        out.present = i;
    }
    if (out.complete())
      break;
  }
  return out;
}

static SwapchainSupport querySwapchainSupport(VkPhysicalDevice gpu,
                                              VkSurfaceKHR surface) {
  SwapchainSupport sup{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &sup.caps);

  uint32_t count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, nullptr);
  sup.formats.resize(count);
  if (count)
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count,
                                         sup.formats.data());

  count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &count, nullptr);
  sup.presentModes.resize(count);
  if (count)
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &count,
                                              sup.presentModes.data());
  return sup;
}

static VkSurfaceFormatKHR
chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats) {
  if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
    return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
  }
  for (auto &f : formats) {
    if ((f.format == VK_FORMAT_B8G8R8A8_SRGB ||
         f.format == VK_FORMAT_B8G8R8A8_UNORM) &&
        f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return f;
    }
  }
  return formats[0];
}

static VkPresentModeKHR
choosePresentMode(const std::vector<VkPresentModeKHR> &modes) {
  for (auto m : modes) {
    if (m == VK_PRESENT_MODE_MAILBOX_KHR)
      return m;
  }
  return VK_PRESENT_MODE_FIFO_KHR; // guaranteed
}

static VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR &caps,
                               SDL_Window *window) {
  if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    return caps.currentExtent;
  }
  int w = 0, h = 0;
  SDL_GetWindowSize(window, &w, &h);
  VkExtent2D e{static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
  e.width = std::max(caps.minImageExtent.width,
                     std::min(caps.maxImageExtent.width, e.width));
  e.height = std::max(caps.minImageExtent.height,
                      std::min(caps.maxImageExtent.height, e.height));
  return e;
}

static VkShaderModule createShaderModule(VkDevice device,
                                         const std::vector<char> &code) {
  VkShaderModuleCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  ci.codeSize = code.size();
  ci.pCode = reinterpret_cast<const uint32_t *>(code.data());
  VkShaderModule mod = VK_NULL_HANDLE;
  VkResult r = vkCreateShaderModule(device, &ci, nullptr, &mod);
  if (r != VK_SUCCESS)
    throw std::runtime_error(std::string("vkCreateShaderModule failed: ") +
                             vk_err(r));
  return mod;
}

Renderer::Renderer() = default;
Renderer::~Renderer() { cleanup(); }

void Renderer::init(SDL_Window *window) {
  try {
    createInstance(window);
    spdlog::info("Vulkan instance created");

    pickDevice();
    spdlog::info("Physical device selected");

    createDevice();
    spdlog::info("Logical device created");

    createSwapchainAndViews(window);
    spdlog::info("Swapchain created: {} images, {}x{}, format {}",
                 (int)vk_.swapImageViews.size(), vk_.swapExtent.width,
                 vk_.swapExtent.height, (int)vk_.swapFormat);

    createRenderPass();
    createPipeline();
    createFramebuffers();
    createCommandsAndSync();
  } catch (const std::exception &ex) {
    spdlog::critical("Vulkan initialization failed: {}", ex.what());
    throw;
  }
}

bool Renderer::drawFrame(SDL_Window *window) {
  VkResult r = vkWaitForFences(vk_.device, 1, &vk_.inFlight, VK_TRUE,
                               UINT64_C(1'000'000'000));
  if (r != VK_SUCCESS) {
    spdlog::error("vkWaitForFences failed: {}", vk_err(r));
    return false;
  }
  vkResetFences(vk_.device, 1, &vk_.inFlight);

  uint32_t imageIndex = 0;
  r = vkAcquireNextImageKHR(vk_.device, vk_.swapchain, UINT64_MAX,
                            vk_.semImageAvailable, VK_NULL_HANDLE, &imageIndex);
  if (r == VK_ERROR_OUT_OF_DATE_KHR) {
    recreateSwapchain(window);
    return true;
  } else if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
    spdlog::error("vkAcquireNextImageKHR failed: {}", vk_err(r));
    return false;
  }

  try {
    recordAndSubmitFrame(imageIndex);
  } catch (const std::exception &ex) {
    spdlog::error("record/submit failed: {}", ex.what());
    return false;
  }

  VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  pi.waitSemaphoreCount = 1;
  pi.pWaitSemaphores = &vk_.semRenderFinished;
  pi.swapchainCount = 1;
  pi.pSwapchains = &vk_.swapchain;
  pi.pImageIndices = &imageIndex;
  r = vkQueuePresentKHR(vk_.presentQueue, &pi);
  if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
    recreateSwapchain(window);
  } else if (r != VK_SUCCESS) {
    spdlog::error("vkQueuePresentKHR failed: {}", vk_err(r));
    return false;
  }

  return true;
}

void Renderer::recreateSwapchain(SDL_Window *window) {
  int w = 0, h = 0;
  SDL_GetWindowSize(window, &w, &h);
  if (w == 0 || h == 0)
    return;

  vkDeviceWaitIdle(vk_.device);
  cleanupSwapchain();
  createSwapchainAndViews(window);
  createRenderPass();
  createPipeline();
  createFramebuffers();
}

void Renderer::cleanup() {
  if (!vk_.instance)
    return;

  if (vk_.device)
    vkDeviceWaitIdle(vk_.device);

  if (vk_.inFlight)
    vkDestroyFence(vk_.device, vk_.inFlight, nullptr);
  if (vk_.semRenderFinished)
    vkDestroySemaphore(vk_.device, vk_.semRenderFinished, nullptr);
  if (vk_.semImageAvailable)
    vkDestroySemaphore(vk_.device, vk_.semImageAvailable, nullptr);

  if (vk_.cmdBuf)
    vkFreeCommandBuffers(vk_.device, vk_.cmdPool, 1, &vk_.cmdBuf);
  if (vk_.cmdPool)
    vkDestroyCommandPool(vk_.device, vk_.cmdPool, nullptr);

  cleanupSwapchain();

  if (vk_.device)
    vkDestroyDevice(vk_.device, nullptr);
  if (vk_.surface)
    vkDestroySurfaceKHR(vk_.instance, vk_.surface, nullptr);

  destroyDebugMessenger(vk_.instance, vk_.debugMessenger);

  if (vk_.instance)
    vkDestroyInstance(vk_.instance, nullptr);

  vk_ = VulkanState{};
}

void Renderer::createInstance(SDL_Window *window) {
  Uint32 extCount = 0;
  const char *const *sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);
  if (!sdlExts || extCount == 0) {
    throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions returned empty");
  }
  std::vector<const char *> extensions(sdlExts, sdlExts + extCount);

  std::vector<const char *> layers;
  if (hasInstanceLayer("VK_LAYER_KHRONOS_validation")) {
    layers.push_back("VK_LAYER_KHRONOS_validation");
    spdlog::info("Enabling validation layer: VK_LAYER_KHRONOS_validation");
  } else {
    spdlog::warn("Validation layer not available: VK_LAYER_KHRONOS_validation");
  }

  bool enableDebugUtils =
      hasInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  if (enableDebugUtils) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    spdlog::info("Enabling instance extension: {}",
                 VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  } else {
    spdlog::warn("Instance extension not available: {}",
                 VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  VkApplicationInfo app{};
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pApplicationName = "Luster";
  app.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  app.pEngineName = "Luster";
  app.engineVersion = VK_MAKE_VERSION(0, 1, 0);
  app.apiVersion = VK_API_VERSION_1_3;

  VkDebugUtilsMessengerCreateInfoEXT dbgInfo{};
  if (enableDebugUtils) {
    fillDebugCreateInfo(dbgInfo);
  }

  VkInstanceCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  ci.pApplicationInfo = &app;
  ci.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  ci.ppEnabledExtensionNames = extensions.data();
  ci.enabledLayerCount = static_cast<uint32_t>(layers.size());
  ci.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();
  ci.pNext = enableDebugUtils ? &dbgInfo : nullptr;

  VkResult r = vkCreateInstance(&ci, nullptr, &vk_.instance);
  if (r != VK_SUCCESS)
    throw std::runtime_error(std::string("vkCreateInstance failed: ") +
                             vk_err(r));

  if (enableDebugUtils) {
    if (setupDebugMessenger(vk_.instance, &vk_.debugMessenger) != VK_SUCCESS) {
      spdlog::warn("Failed to create debug messenger");
    }
  }

  if (!SDL_Vulkan_CreateSurface(window, vk_.instance, nullptr, &vk_.surface) ||
      vk_.surface == VK_NULL_HANDLE) {
    throw std::runtime_error("SDL_Vulkan_CreateSurface failed");
  }
}

void Renderer::pickDevice() {
  uint32_t count = 0;
  vkEnumeratePhysicalDevices(vk_.instance, &count, nullptr);
  if (!count)
    throw std::runtime_error("No Vulkan physical devices");
  std::vector<VkPhysicalDevice> gpus(count);
  vkEnumeratePhysicalDevices(vk_.instance, &count, gpus.data());

  for (auto d : gpus) {
    QueueFamilies q = findQueueFamilies(d, vk_.surface);
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(d, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> devExts(extCount);
    vkEnumerateDeviceExtensionProperties(d, nullptr, &extCount, devExts.data());
    bool hasSwapchain = false;
    for (auto &e : devExts)
      if (std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
        hasSwapchain = true;

    if (q.complete() && hasSwapchain) {
      vk_.gpu = d;
      vk_.gfxQueueFamily = q.graphics.value();
      vk_.presentQueueFamily = q.present.value();
      return;
    }
  }
  throw std::runtime_error("Failed to find suitable GPU");
}

void Renderer::createDevice() {
  float prio = 1.0f;
  std::vector<VkDeviceQueueCreateInfo> qcis;
  std::array<uint32_t, 2> unique = {vk_.gfxQueueFamily, vk_.presentQueueFamily};
  std::sort(unique.begin(), unique.end());
  auto last = std::unique(unique.begin(), unique.end());
  for (auto it = unique.begin(); it != last; ++it) {
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = *it;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;
    qcis.push_back(qci);
  }

  VkPhysicalDeviceFeatures feats{};

  const char *extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  VkDeviceCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  ci.queueCreateInfoCount = static_cast<uint32_t>(qcis.size());
  ci.pQueueCreateInfos = qcis.data();
  ci.pEnabledFeatures = &feats;
  ci.enabledExtensionCount = 1;
  ci.ppEnabledExtensionNames = extensions;

  VkResult r = vkCreateDevice(vk_.gpu, &ci, nullptr, &vk_.device);
  if (r != VK_SUCCESS)
    throw std::runtime_error(std::string("vkCreateDevice failed: ") +
                             vk_err(r));

  vkGetDeviceQueue(vk_.device, vk_.gfxQueueFamily, 0, &vk_.gfxQueue);
  vkGetDeviceQueue(vk_.device, vk_.presentQueueFamily, 0, &vk_.presentQueue);
}

void Renderer::createSwapchainAndViews(SDL_Window *window) {
  SwapchainSupport sup = querySwapchainSupport(vk_.gpu, vk_.surface);
  VkSurfaceFormatKHR chosenFormat = chooseSurfaceFormat(sup.formats);
  VkPresentModeKHR chosenPresent = choosePresentMode(sup.presentModes);
  VkExtent2D extent = chooseExtent(sup.caps, window);

  uint32_t imageCount = sup.caps.minImageCount + 1;
  if (sup.caps.maxImageCount > 0 && imageCount > sup.caps.maxImageCount) {
    imageCount = sup.caps.maxImageCount;
  }

  VkSwapchainCreateInfoKHR sci{};
  sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  sci.surface = vk_.surface;
  sci.minImageCount = imageCount;
  sci.imageFormat = chosenFormat.format;
  sci.imageColorSpace = chosenFormat.colorSpace;
  sci.imageExtent = extent;
  sci.imageArrayLayers = 1;
  sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  uint32_t queueFamilyIndices[] = {vk_.gfxQueueFamily, vk_.presentQueueFamily};
  if (vk_.gfxQueueFamily != vk_.presentQueueFamily) {
    sci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    sci.queueFamilyIndexCount = 2;
    sci.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  sci.preTransform = sup.caps.currentTransform;
  sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  sci.presentMode = chosenPresent;
  sci.clipped = VK_TRUE;

  VkResult r = vkCreateSwapchainKHR(vk_.device, &sci, nullptr, &vk_.swapchain);
  if (r != VK_SUCCESS)
    throw std::runtime_error(std::string("vkCreateSwapchainKHR failed: ") +
                             vk_err(r));

  vk_.swapFormat = chosenFormat.format;
  vk_.swapColorSpace = chosenFormat.colorSpace;
  vk_.swapExtent = extent;

  vkGetSwapchainImagesKHR(vk_.device, vk_.swapchain, &imageCount, nullptr);
  vk_.swapImages.resize(imageCount);
  vkGetSwapchainImagesKHR(vk_.device, vk_.swapchain, &imageCount,
                          vk_.swapImages.data());

  vk_.swapImageViews.resize(vk_.swapImages.size());
  for (size_t i = 0; i < vk_.swapImages.size(); ++i) {
    VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ci.image = vk_.swapImages[i];
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = vk_.swapFormat;
    ci.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ci.subresourceRange.levelCount = 1;
    ci.subresourceRange.layerCount = 1;
    VkResult res =
        vkCreateImageView(vk_.device, &ci, nullptr, &vk_.swapImageViews[i]);
    if (res != VK_SUCCESS)
      throw std::runtime_error(std::string("vkCreateImageView failed: ") +
                               vk_err(res));
  }
}

void Renderer::createRenderPass() {
  VkAttachmentDescription color{};
  color.format = vk_.swapFormat;
  color.samples = VK_SAMPLE_COUNT_1_BIT;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorRef{};
  colorRef.attachment = 0;
  colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription sub{};
  sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  sub.colorAttachmentCount = 1;
  sub.pColorAttachments = &colorRef;

  VkSubpassDependency dep{};
  dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  dep.dstSubpass = 0;
  dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dep.srcAccessMask = 0;
  dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo rpci{};
  rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  rpci.attachmentCount = 1;
  rpci.pAttachments = &color;
  rpci.subpassCount = 1;
  rpci.pSubpasses = &sub;
  rpci.dependencyCount = 1;
  rpci.pDependencies = &dep;

  VkResult r = vkCreateRenderPass(vk_.device, &rpci, nullptr, &vk_.renderPass);
  if (r != VK_SUCCESS)
    throw std::runtime_error(std::string("vkCreateRenderPass failed: ") +
                             vk_err(r));
}

void Renderer::createPipeline() {
  const std::string vsPath = "shaders/triangle.vert.spv";
  const std::string fsPath = "shaders/triangle.frag.spv";
  auto vsCode = readFileBinary(vsPath);
  auto fsCode = readFileBinary(fsPath);

  VkShaderModule vs = createShaderModule(vk_.device, vsCode);
  VkShaderModule fs = createShaderModule(vk_.device, fsCode);

  VkPipelineShaderStageCreateInfo vsStage{};
  vsStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vsStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vsStage.module = vs;
  vsStage.pName = "main";

  VkPipelineShaderStageCreateInfo fsStage{};
  fsStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fsStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fsStage.module = fs;
  fsStage.pName = "main";

  VkPipelineShaderStageCreateInfo stages[] = {vsStage, fsStage};

  VkPipelineVertexInputStateCreateInfo vi{};
  vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vi.vertexBindingDescriptionCount = 0;
  vi.pVertexBindingDescriptions = nullptr;
  vi.vertexAttributeDescriptionCount = 0;
  vi.pVertexAttributeDescriptions = nullptr;

  VkPipelineInputAssemblyStateCreateInfo ia{};
  ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  ia.primitiveRestartEnable = VK_FALSE;

  VkViewport vp{};
  vp.x = 0.f;
  vp.y = 0.f;
  vp.width = static_cast<float>(vk_.swapExtent.width);
  vp.height = static_cast<float>(vk_.swapExtent.height);
  vp.minDepth = 0.f;
  vp.maxDepth = 1.f;

  VkRect2D sc{};
  sc.offset = {0, 0};
  sc.extent = vk_.swapExtent;

  VkPipelineViewportStateCreateInfo vpState{};
  vpState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  vpState.viewportCount = 1;
  vpState.pViewports = &vp;
  vpState.scissorCount = 1;
  vpState.pScissors = &sc;

  VkPipelineRasterizationStateCreateInfo rs{};
  rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rs.depthClampEnable = VK_FALSE;
  rs.rasterizerDiscardEnable = VK_FALSE;
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode = VK_CULL_MODE_BACK_BIT;
  rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rs.depthBiasEnable = VK_FALSE;
  rs.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo ms{};
  ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  ms.sampleShadingEnable = VK_FALSE;

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo cb{};
  cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  cb.attachmentCount = 1;
  cb.pAttachments = &colorBlendAttachment;

  VkPipelineLayoutCreateInfo pl{};
  pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

  VkResult r =
      vkCreatePipelineLayout(vk_.device, &pl, nullptr, &vk_.pipelineLayout);
  if (r != VK_SUCCESS)
    throw std::runtime_error(std::string("vkCreatePipelineLayout failed: ") +
                             vk_err(r));

  VkGraphicsPipelineCreateInfo pci{};
  pci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pci.stageCount = 2;
  pci.pStages = stages;
  pci.pVertexInputState = &vi;
  pci.pInputAssemblyState = &ia;
  pci.pViewportState = &vpState;
  pci.pRasterizationState = &rs;
  pci.pMultisampleState = &ms;
  pci.pDepthStencilState = nullptr;
  pci.pColorBlendState = &cb;
  pci.pDynamicState = nullptr;
  pci.layout = vk_.pipelineLayout;
  pci.renderPass = vk_.renderPass;
  pci.subpass = 0;

  r = vkCreateGraphicsPipelines(vk_.device, VK_NULL_HANDLE, 1, &pci, nullptr,
                                &vk_.pipeline);
  if (r != VK_SUCCESS)
    throw std::runtime_error(std::string("vkCreateGraphicsPipelines failed: ") +
                             vk_err(r));

  vkDestroyShaderModule(vk_.device, fs, nullptr);
  vkDestroyShaderModule(vk_.device, vs, nullptr);
}

void Renderer::createFramebuffers() {
  vk_.framebuffers.resize(vk_.swapImageViews.size());
  for (size_t i = 0; i < vk_.swapImageViews.size(); ++i) {
    VkImageView attachments[] = {vk_.swapImageViews[i]};
    VkFramebufferCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fci.renderPass = vk_.renderPass;
    fci.attachmentCount = 1;
    fci.pAttachments = attachments;
    fci.width = vk_.swapExtent.width;
    fci.height = vk_.swapExtent.height;
    fci.layers = 1;
    VkResult r =
        vkCreateFramebuffer(vk_.device, &fci, nullptr, &vk_.framebuffers[i]);
    if (r != VK_SUCCESS)
      throw std::runtime_error(std::string("vkCreateFramebuffer failed: ") +
                               vk_err(r));
  }
}

void Renderer::createCommandsAndSync() {
  VkCommandPoolCreateInfo pci{};
  pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pci.queueFamilyIndex = vk_.gfxQueueFamily;
  VkResult r = vkCreateCommandPool(vk_.device, &pci, nullptr, &vk_.cmdPool);
  if (r != VK_SUCCESS)
    throw std::runtime_error(std::string("vkCreateCommandPool failed: ") +
                             vk_err(r));

  VkCommandBufferAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  ai.commandPool = vk_.cmdPool;
  ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = 1;
  r = vkAllocateCommandBuffers(vk_.device, &ai, &vk_.cmdBuf);
  if (r != VK_SUCCESS)
    throw std::runtime_error(std::string("vkAllocateCommandBuffers failed: ") +
                             vk_err(r));

  VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  r = vkCreateSemaphore(vk_.device, &sci, nullptr, &vk_.semImageAvailable);
  if (r != VK_SUCCESS)
    throw std::runtime_error(
        std::string("vkCreateSemaphore (imageAvailable) failed: ") + vk_err(r));
  r = vkCreateSemaphore(vk_.device, &sci, nullptr, &vk_.semRenderFinished);
  if (r != VK_SUCCESS)
    throw std::runtime_error(
        std::string("vkCreateSemaphore (renderFinished) failed: ") + vk_err(r));

  VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  r = vkCreateFence(vk_.device, &fci, nullptr, &vk_.inFlight);
  if (r != VK_SUCCESS)
    throw std::runtime_error(std::string("vkCreateFence failed: ") + vk_err(r));
}

void Renderer::cleanupSwapchain() {
  for (auto fb : vk_.framebuffers)
    if (fb)
      vkDestroyFramebuffer(vk_.device, fb, nullptr);
  vk_.framebuffers.clear();

  if (vk_.pipeline)
    vkDestroyPipeline(vk_.device, vk_.pipeline, nullptr);
  vk_.pipeline = VK_NULL_HANDLE;
  if (vk_.pipelineLayout)
    vkDestroyPipelineLayout(vk_.device, vk_.pipelineLayout, nullptr);
  vk_.pipelineLayout = VK_NULL_HANDLE;
  if (vk_.renderPass)
    vkDestroyRenderPass(vk_.device, vk_.renderPass, nullptr);
  vk_.renderPass = VK_NULL_HANDLE;

  for (auto iv : vk_.swapImageViews)
    if (iv)
      vkDestroyImageView(vk_.device, iv, nullptr);
  vk_.swapImageViews.clear();

  if (vk_.swapchain)
    vkDestroySwapchainKHR(vk_.device, vk_.swapchain, nullptr);
  vk_.swapchain = VK_NULL_HANDLE;
}

void Renderer::recordAndSubmitFrame(uint32_t imageIndex) {
  vkResetCommandBuffer(vk_.cmdBuf, 0);
  VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  VkResult r = vkBeginCommandBuffer(vk_.cmdBuf, &bi);
  if (r != VK_SUCCESS)
    throw std::runtime_error(std::string("vkBeginCommandBuffer failed: ") +
                             vk_err(r));

  VkClearValue clear{};
  clear.color = {{0.05f, 0.06f, 0.09f, 1.0f}};

  VkRenderPassBeginInfo rpbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  rpbi.renderPass = vk_.renderPass;
  rpbi.framebuffer = vk_.framebuffers[imageIndex];
  rpbi.renderArea.offset = {0, 0};
  rpbi.renderArea.extent = vk_.swapExtent;
  rpbi.clearValueCount = 1;
  rpbi.pClearValues = &clear;

  vkCmdBeginRenderPass(vk_.cmdBuf, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(vk_.cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_.pipeline);

  vkCmdDraw(vk_.cmdBuf, 3, 1, 0, 0);
  vkCmdEndRenderPass(vk_.cmdBuf);

  r = vkEndCommandBuffer(vk_.cmdBuf);
  if (r != VK_SUCCESS)
    throw std::runtime_error(std::string("vkEndCommandBuffer failed: ") +
                             vk_err(r));

  VkPipelineStageFlags waitStages =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  si.waitSemaphoreCount = 1;
  si.pWaitSemaphores = &vk_.semImageAvailable;
  si.pWaitDstStageMask = &waitStages;
  si.commandBufferCount = 1;
  si.pCommandBuffers = &vk_.cmdBuf;
  si.signalSemaphoreCount = 1;
  si.pSignalSemaphores = &vk_.semRenderFinished;

  r = vkQueueSubmit(vk_.gfxQueue, 1, &si, vk_.inFlight);
  if (r != VK_SUCCESS)
    throw std::runtime_error(std::string("vkQueueSubmit failed: ") + vk_err(r));
}

} // namespace luster
