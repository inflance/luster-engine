#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_main.h>

#include <spdlog/spdlog.h>
#include <vulkan/vulkan.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// Helpers for logging Vulkan errors
static void log_sdl_error(const char* what) {
    const char* err = SDL_GetError();
    spdlog::error("{}: {}", what, (err && *err) ? err : "unknown");
}
static const char* vk_err(VkResult r) {
    switch (r) {
#define E(x) case x: return #x
        E(VK_SUCCESS);
        E(VK_NOT_READY);
        E(VK_TIMEOUT);
        E(VK_EVENT_SET);
        E(VK_EVENT_RESET);
        E(VK_INCOMPLETE);
        E(VK_ERROR_OUT_OF_HOST_MEMORY);
        E(VK_ERROR_OUT_OF_DEVICE_MEMORY);
        E(VK_ERROR_INITIALIZATION_FAILED);
        E(VK_ERROR_DEVICE_LOST);
        E(VK_ERROR_MEMORY_MAP_FAILED);
        E(VK_ERROR_LAYER_NOT_PRESENT);
        E(VK_ERROR_EXTENSION_NOT_PRESENT);
        E(VK_ERROR_FEATURE_NOT_PRESENT);
        E(VK_ERROR_INCOMPATIBLE_DRIVER);
        E(VK_ERROR_TOO_MANY_OBJECTS);
        E(VK_ERROR_FORMAT_NOT_SUPPORTED);
        E(VK_ERROR_SURFACE_LOST_KHR);
        E(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
        E(VK_SUBOPTIMAL_KHR);
        E(VK_ERROR_OUT_OF_DATE_KHR);
        E(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
        E(VK_ERROR_VALIDATION_FAILED_EXT);
        E(VK_ERROR_INVALID_SHADER_NV);
#undef E
   default: return "VK_UNKNOWN";
   }
}

 // ---- Debug utils & validation helpers ----
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*                                       pUserData) {
    (void)messageTypes; (void)pUserData;
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        spdlog::error("[Vulkan] {}", pCallbackData ? pCallbackData->pMessage : "(null)");
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        spdlog::warn("[Vulkan] {}", pCallbackData ? pCallbackData->pMessage : "(null)");
    } else {
        spdlog::info("[Vulkan] {}", pCallbackData ? pCallbackData->pMessage : "(null)");
    }
    return VK_FALSE;
}

static void fillDebugCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& info) {
    info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = debugCallback;
}

static bool hasInstanceLayer(const char* name) {
    uint32_t count = 0;
    if (vkEnumerateInstanceLayerProperties(&count, nullptr) != VK_SUCCESS) return false;
    std::vector<VkLayerProperties> layers(count);
    if (count) vkEnumerateInstanceLayerProperties(&count, layers.data());
    for (auto& lp : layers) {
        if (std::strcmp(lp.layerName, name) == 0) return true;
    }
    return false;
}

static bool hasInstanceExtension(const char* name) {
    uint32_t count = 0;
    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS) return false;
    std::vector<VkExtensionProperties> exts(count);
    if (count) vkEnumerateInstanceExtensionProperties(nullptr, &count, exts.data());
    for (auto& ep : exts) {
        if (std::strcmp(ep.extensionName, name) == 0) return true;
    }
    return false;
}

static VkResult setupDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    auto fpCreate = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (!fpCreate) return VK_ERROR_EXTENSION_NOT_PRESENT;
    VkDebugUtilsMessengerCreateInfoEXT info{};
    fillDebugCreateInfo(info);
    return fpCreate(instance, &info, nullptr, out);
}

static void destroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT msgr) {
    if (!msgr) return;
    auto fpDestroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (fpDestroy) fpDestroy(instance, msgr, nullptr);
}

struct QueueFamilies {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;
    bool complete() const { return graphics.has_value() && present.has_value(); }
};

struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR        caps{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;
};

static std::vector<char> readFileBinary(const std::string& path) {
    std::ifstream f(path, std::ios::ate | std::ios::binary);
    if (!f) throw std::runtime_error("Failed to open file: " + path);
    const size_t size = static_cast<size_t>(f.tellg());
    std::vector<char> buffer(size);
    f.seekg(0);
    if (!f.read(buffer.data(), buffer.size())) {
        throw std::runtime_error("Failed to read file: " + path);
    }
    return buffer;
}

static QueueFamilies findQueueFamilies(VkPhysicalDevice gpu, VkSurfaceKHR surface) {
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
            if (presentSupport) out.present = i;
        }
        if (out.complete()) break;
    }
    return out;
}

static SwapchainSupport querySwapchainSupport(VkPhysicalDevice gpu, VkSurfaceKHR surface) {
    SwapchainSupport sup{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &sup.caps);

    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, nullptr);
    sup.formats.resize(count);
    if (count) vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, sup.formats.data());

    count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &count, nullptr);
    sup.presentModes.resize(count);
    if (count) vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &count, sup.presentModes.data());
    return sup;
}

static VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }
    for (auto& f : formats) {
        if ((f.format == VK_FORMAT_B8G8R8A8_SRGB || f.format == VK_FORMAT_B8G8R8A8_UNORM) &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    return formats[0];
}

static VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes) {
    for (auto m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    }
    return VK_PRESENT_MODE_FIFO_KHR; // guaranteed
}

static VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& caps, SDL_Window* window) {
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return caps.currentExtent;
    }
    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);
    VkExtent2D e{static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
    e.width  = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, e.width));
    e.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, e.height));
    return e;
}

static VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode    = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule mod = VK_NULL_HANDLE;
    VkResult r = vkCreateShaderModule(device, &ci, nullptr, &mod);
    if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkCreateShaderModule failed: ") + vk_err(r));
    return mod;
}

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
    VkFence     inFlight = VK_NULL_HANDLE;

    bool framebufferResized = false;

    // Debug utils
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
};

static void createInstance(SDL_Window* window, VulkanState& vk) {
    // Query SDL required instance extensions
    Uint32 extCount = 0;
    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);
    if (!sdlExts || extCount == 0) {
        throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions returned empty");
    }
    std::vector<const char*> extensions(sdlExts, sdlExts + extCount);

    // Validation layer and debug utils
    std::vector<const char*> layers;
    if (hasInstanceLayer("VK_LAYER_KHRONOS_validation")) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
        spdlog::info("Enabling validation layer: VK_LAYER_KHRONOS_validation");
    } else {
        spdlog::warn("Validation layer not available: VK_LAYER_KHRONOS_validation");
    }

    bool enableDebugUtils = hasInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (enableDebugUtils) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        spdlog::info("Enabling instance extension: {}", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    } else {
        spdlog::warn("Instance extension not available: {}", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "Luster";
    app.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app.pEngineName = "Luster";
    app.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app.apiVersion = VK_API_VERSION_1_3; // works with SDK 1.3+; driver may expose 1.3/1.4

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

    VkResult r = vkCreateInstance(&ci, nullptr, &vk.instance);
    if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkCreateInstance failed: ") + vk_err(r));

    // Setup debug messenger after instance creation
    if (enableDebugUtils) {
        if (setupDebugMessenger(vk.instance, &vk.debugMessenger) != VK_SUCCESS) {
            spdlog::warn("Failed to create debug messenger");
        }
    }

    // Surface (via SDL)
    if (!SDL_Vulkan_CreateSurface(window, vk.instance, nullptr, &vk.surface) || vk.surface == VK_NULL_HANDLE) {
        throw std::runtime_error("SDL_Vulkan_CreateSurface failed");
    }
}

static void pickDevice(VulkanState& vk) {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(vk.instance, &count, nullptr);
    if (!count) throw std::runtime_error("No Vulkan physical devices");
    std::vector<VkPhysicalDevice> gpus(count);
    vkEnumeratePhysicalDevices(vk.instance, &count, gpus.data());

    for (auto d : gpus) {
        QueueFamilies q = findQueueFamilies(d, vk.surface);
        // Require swapchain extension
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(d, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> devExts(extCount);
        vkEnumerateDeviceExtensionProperties(d, nullptr, &extCount, devExts.data());
        bool hasSwapchain = false;
        for (auto& e : devExts) if (std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) hasSwapchain = true;

        if (q.complete() && hasSwapchain) {
            vk.gpu = d;
            vk.gfxQueueFamily = q.graphics.value();
            vk.presentQueueFamily = q.present.value();
            return;
        }
    }
    throw std::runtime_error("Failed to find suitable GPU with graphics+present+swapchain");
}

static void createDevice(VulkanState& vk) {
    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> qcis;
    std::vector<uint32_t> families{vk.gfxQueueFamily, vk.presentQueueFamily};
    // unique families
    std::sort(families.begin(), families.end());
    families.erase(std::unique(families.begin(), families.end()), families.end());

    for (uint32_t fam : families) {
        VkDeviceQueueCreateInfo qci{};
        qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = fam;
        qci.queueCount = 1;
        qci.pQueuePriorities = &priority;
        qcis.push_back(qci);
    }

    VkPhysicalDeviceFeatures features{}; // default

    const char* devExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = static_cast<uint32_t>(qcis.size());
    dci.pQueueCreateInfos = qcis.data();
    dci.pEnabledFeatures = &features;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = devExts;

    VkResult r = vkCreateDevice(vk.gpu, &dci, nullptr, &vk.device);
    if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkCreateDevice failed: ") + vk_err(r));

    // volkLoadDevice removed (using Vulkan loader trampolines)

    vkGetDeviceQueue(vk.device, vk.gfxQueueFamily, 0, &vk.gfxQueue);
    vkGetDeviceQueue(vk.device, vk.presentQueueFamily, 0, &vk.presentQueue);
}

static void createSwapchainAndViews(SDL_Window* window, VulkanState& vk) {
    SwapchainSupport sup = querySwapchainSupport(vk.gpu, vk.surface);
    VkSurfaceFormatKHR fmt = chooseSurfaceFormat(sup.formats);
    VkPresentModeKHR mode = choosePresentMode(sup.presentModes);
    VkExtent2D extent = chooseExtent(sup.caps, window);
    uint32_t imageCount = sup.caps.minImageCount + 1;
    if (sup.caps.maxImageCount > 0) imageCount = std::min(imageCount, sup.caps.maxImageCount);

    VkSwapchainCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = vk.surface;
    sci.minImageCount = imageCount;
    sci.imageFormat = fmt.format;
    sci.imageColorSpace = fmt.colorSpace;
    sci.imageExtent = extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = { vk.gfxQueueFamily, vk.presentQueueFamily };
    if (vk.gfxQueueFamily != vk.presentQueueFamily) {
        sci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        sci.queueFamilyIndexCount = 2;
        sci.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    sci.preTransform = sup.caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = mode;
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = VK_NULL_HANDLE;

    VkResult r = vkCreateSwapchainKHR(vk.device, &sci, nullptr, &vk.swapchain);
    if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkCreateSwapchainKHR failed: ") + vk_err(r));

    vk.swapFormat = fmt.format;
    vk.swapColorSpace = fmt.colorSpace;
    vk.swapExtent = extent;

    uint32_t count = 0;
    vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &count, nullptr);
    vk.swapImages.resize(count);
    vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &count, vk.swapImages.data());

    // Views
    vk.swapImageViews.resize(vk.swapImages.size());
    for (size_t i = 0; i < vk.swapImages.size(); ++i) {
        VkImageViewCreateInfo iv{};
        iv.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        iv.image = vk.swapImages[i];
        iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
        iv.format = vk.swapFormat;
        iv.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                          VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        iv.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        iv.subresourceRange.baseMipLevel = 0;
        iv.subresourceRange.levelCount = 1;
        iv.subresourceRange.baseArrayLayer = 0;
        iv.subresourceRange.layerCount = 1;
        r = vkCreateImageView(vk.device, &iv, nullptr, &vk.swapImageViews[i]);
        if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkCreateImageView failed: ") + vk_err(r));
    }
}

static void createRenderPass(VulkanState& vk) {
    VkAttachmentDescription color{};
    color.format = vk.swapFormat;
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

    VkResult r = vkCreateRenderPass(vk.device, &rpci, nullptr, &vk.renderPass);
    if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkCreateRenderPass failed: ") + vk_err(r));
}

static void createPipeline(VulkanState& vk) {
    const std::string vsPath = "shaders/triangle.vert.spv";
    const std::string fsPath = "shaders/triangle.frag.spv";
    auto vsCode = readFileBinary(vsPath);
    auto fsCode = readFileBinary(fsPath);

    VkShaderModule vs = createShaderModule(vk.device, vsCode);
    VkShaderModule fs = createShaderModule(vk.device, fsCode);

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

    VkPipelineShaderStageCreateInfo stages[] = { vsStage, fsStage };

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
    vp.x = 0.f; vp.y = 0.f;
    vp.width = static_cast<float>(vk.swapExtent.width);
    vp.height = static_cast<float>(vk.swapExtent.height);
    vp.minDepth = 0.f; vp.maxDepth = 1.f;

    VkRect2D sc{};
    sc.offset = {0,0};
    sc.extent = vk.swapExtent;

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

    VkResult r = vkCreatePipelineLayout(vk.device, &pl, nullptr, &vk.pipelineLayout);
    if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkCreatePipelineLayout failed: ") + vk_err(r));

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
    pci.layout = vk.pipelineLayout;
    pci.renderPass = vk.renderPass;
    pci.subpass = 0;

    r = vkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &pci, nullptr, &vk.pipeline);
    if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkCreateGraphicsPipelines failed: ") + vk_err(r));

    vkDestroyShaderModule(vk.device, fs, nullptr);
    vkDestroyShaderModule(vk.device, vs, nullptr);
}

static void createFramebuffers(VulkanState& vk) {
    vk.framebuffers.resize(vk.swapImageViews.size());
    for (size_t i = 0; i < vk.swapImageViews.size(); ++i) {
        VkImageView attachments[] = { vk.swapImageViews[i] };
        VkFramebufferCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass = vk.renderPass;
        fci.attachmentCount = 1;
        fci.pAttachments = attachments;
        fci.width = vk.swapExtent.width;
        fci.height = vk.swapExtent.height;
        fci.layers = 1;
        VkResult r = vkCreateFramebuffer(vk.device, &fci, nullptr, &vk.framebuffers[i]);
        if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkCreateFramebuffer failed: ") + vk_err(r));
    }
}

static void createCommandsAndSync(VulkanState& vk) {
    VkCommandPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = vk.gfxQueueFamily;
    VkResult r = vkCreateCommandPool(vk.device, &pci, nullptr, &vk.cmdPool);
    if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkCreateCommandPool failed: ") + vk_err(r));

    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = vk.cmdPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    r = vkAllocateCommandBuffers(vk.device, &ai, &vk.cmdBuf);
    if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkAllocateCommandBuffers failed: ") + vk_err(r));

    VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    r = vkCreateSemaphore(vk.device, &sci, nullptr, &vk.semImageAvailable);
    if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkCreateSemaphore (imageAvailable) failed: ") + vk_err(r));
    r = vkCreateSemaphore(vk.device, &sci, nullptr, &vk.semRenderFinished);
    if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkCreateSemaphore (renderFinished) failed: ") + vk_err(r));

    VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    r = vkCreateFence(vk.device, &fci, nullptr, &vk.inFlight);
    if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkCreateFence failed: ") + vk_err(r));
}

static void cleanupSwapchain(VulkanState& vk) {
    for (auto fb : vk.framebuffers) if (fb) vkDestroyFramebuffer(vk.device, fb, nullptr);
    vk.framebuffers.clear();

    if (vk.pipeline) vkDestroyPipeline(vk.device, vk.pipeline, nullptr);
    vk.pipeline = VK_NULL_HANDLE;
    if (vk.pipelineLayout) vkDestroyPipelineLayout(vk.device, vk.pipelineLayout, nullptr);
    vk.pipelineLayout = VK_NULL_HANDLE;
    if (vk.renderPass) vkDestroyRenderPass(vk.device, vk.renderPass, nullptr);
    vk.renderPass = VK_NULL_HANDLE;

    for (auto iv : vk.swapImageViews) if (iv) vkDestroyImageView(vk.device, iv, nullptr);
    vk.swapImageViews.clear();

    if (vk.swapchain) vkDestroySwapchainKHR(vk.device, vk.swapchain, nullptr);
    vk.swapchain = VK_NULL_HANDLE;
}

static void recreateSwapchain(SDL_Window* window, VulkanState& vk) {
    int w=0,h=0;
    SDL_GetWindowSize(window, &w, &h);
    if (w == 0 || h == 0) return; // minimized

    vkDeviceWaitIdle(vk.device);
    cleanupSwapchain(vk);
    createSwapchainAndViews(window, vk);
    createRenderPass(vk);
    createPipeline(vk);
    createFramebuffers(vk);
}

static void recordAndSubmitFrame(VulkanState& vk, uint32_t imageIndex) {
    // Reset cmd buffer and record
    vkResetCommandBuffer(vk.cmdBuf, 0);
    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    VkResult r = vkBeginCommandBuffer(vk.cmdBuf, &bi);
    if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkBeginCommandBuffer failed: ") + vk_err(r));

    VkClearValue clear{};
    clear.color = { { 0.05f, 0.06f, 0.09f, 1.0f } };

    VkRenderPassBeginInfo rpbi{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    rpbi.renderPass = vk.renderPass;
    rpbi.framebuffer = vk.framebuffers[imageIndex];
    rpbi.renderArea.offset = {0,0};
    rpbi.renderArea.extent = vk.swapExtent;
    rpbi.clearValueCount = 1;
    rpbi.pClearValues = &clear;

    vkCmdBeginRenderPass(vk.cmdBuf, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(vk.cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline);
    // Viewport/scissor are baked in the pipeline; no dynamic state calls here.

    vkCmdDraw(vk.cmdBuf, 3, 1, 0, 0);
    vkCmdEndRenderPass(vk.cmdBuf);

    r = vkEndCommandBuffer(vk.cmdBuf);
    if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkEndCommandBuffer failed: ") + vk_err(r));

    VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &vk.semImageAvailable;
    si.pWaitDstStageMask = &waitStages;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &vk.cmdBuf;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &vk.semRenderFinished;

    r = vkQueueSubmit(vk.gfxQueue, 1, &si, vk.inFlight);
    if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkQueueSubmit failed: ") + vk_err(r));
}

int main() {
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::info("Luster sandbox starting (SDL + Vulkan triangle)...");

#if defined(_WIN32)
    SDL_RegisterApp("Luster", 0, GetModuleHandleW(nullptr));
#endif

    // Using Vulkan loader; no volk initialization required.

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        log_sdl_error("SDL_Init(SDL_INIT_VIDEO) failed");
        return EXIT_FAILURE;
    }

    const int width = 1280, height = 720;
    SDL_Window* window = SDL_CreateWindow("Luster (Vulkan)", width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        log_sdl_error("SDL_CreateWindow failed");
        SDL_Quit();
        return EXIT_FAILURE;
    }

    VulkanState vk{};

    try {
        createInstance(window, vk);
        spdlog::info("Vulkan instance created");

        pickDevice(vk);
        spdlog::info("Physical device selected");

        createDevice(vk);
        spdlog::info("Logical device created");

        createSwapchainAndViews(window, vk);
        spdlog::info("Swapchain created: {} images, {}x{}, format {}", (int)vk.swapImageViews.size(),
                     vk.swapExtent.width, vk.swapExtent.height, (int)vk.swapFormat);

        createRenderPass(vk);
        createPipeline(vk);
        createFramebuffers(vk);
        createCommandsAndSync(vk);
    } catch (const std::exception& ex) {
        spdlog::critical("Initialization failed: {}", ex.what());
        if (vk.device) vkDeviceWaitIdle(vk.device);
        if (vk.inFlight) vkDestroyFence(vk.device, vk.inFlight, nullptr);
        if (vk.semRenderFinished) vkDestroySemaphore(vk.device, vk.semRenderFinished, nullptr);
        if (vk.semImageAvailable) vkDestroySemaphore(vk.device, vk.semImageAvailable, nullptr);
        if (vk.cmdBuf) vkFreeCommandBuffers(vk.device, vk.cmdPool, 1, &vk.cmdBuf);
        if (vk.cmdPool) vkDestroyCommandPool(vk.device, vk.cmdPool, nullptr);
        cleanupSwapchain(vk);
        if (vk.device) vkDestroyDevice(vk.device, nullptr);
        if (vk.surface) vkDestroySurfaceKHR(vk.instance, vk.surface, nullptr);
        destroyDebugMessenger(vk.instance, vk.debugMessenger);
        if (vk.surface) vkDestroySurfaceKHR(vk.instance, vk.surface, nullptr);
        if (vk.instance) vkDestroyInstance(vk.instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_EVENT_QUIT:
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    running = false;
                    break;
                case SDL_EVENT_WINDOW_RESIZED:
                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    vk.framebufferResized = true;
                    break;
                default:
                    break;
            }
        }

        if (vk.framebufferResized) {
            vk.framebufferResized = false;
            try {
                recreateSwapchain(window, vk);
            } catch (const std::exception& ex) {
                spdlog::error("recreateSwapchain failed: {}", ex.what());
                running = false;
                break;
            }
        }

        // Wait for previous frame to complete before starting new one
        VkResult r = vkWaitForFences(vk.device, 1, &vk.inFlight, VK_TRUE, UINT64_C(1'000'000'000));
        if (r != VK_SUCCESS) {
            spdlog::error("vkWaitForFences failed: {}", vk_err(r));
            running = false;
            break;
        }
        vkResetFences(vk.device, 1, &vk.inFlight);

        // Acquire image
        uint32_t imageIndex = 0;
        r = vkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX, vk.semImageAvailable, VK_NULL_HANDLE, &imageIndex);
        if (r == VK_ERROR_OUT_OF_DATE_KHR) {
            vk.framebufferResized = true;
            continue;
        } else if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
            spdlog::error("vkAcquireNextImageKHR failed: {}", vk_err(r));
            running = false;
            break;
        }

        try {
            recordAndSubmitFrame(vk, imageIndex);
        } catch (const std::exception& ex) {
            spdlog::error("record/submit failed: {}", ex.what());
            running = false;
            break;
        }

        // Present
        VkPresentInfoKHR pi{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores = &vk.semRenderFinished;
        pi.swapchainCount = 1;
        pi.pSwapchains = &vk.swapchain;
        pi.pImageIndices = &imageIndex;
        r = vkQueuePresentKHR(vk.presentQueue, &pi);
        if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
            vk.framebufferResized = true;
        } else if (r != VK_SUCCESS) {
            spdlog::error("vkQueuePresentKHR failed: {}", vk_err(r));
            running = false;
            break;
        }

        // Basic pacing
        SDL_Delay(1);
    }

    // Cleanup
    vkDeviceWaitIdle(vk.device);

    if (vk.inFlight) vkDestroyFence(vk.device, vk.inFlight, nullptr);
    if (vk.semRenderFinished) vkDestroySemaphore(vk.device, vk.semRenderFinished, nullptr);
    if (vk.semImageAvailable) vkDestroySemaphore(vk.device, vk.semImageAvailable, nullptr);

    if (vk.cmdBuf) vkFreeCommandBuffers(vk.device, vk.cmdPool, 1, &vk.cmdBuf);
    if (vk.cmdPool) vkDestroyCommandPool(vk.device, vk.cmdPool, nullptr);

    cleanupSwapchain(vk);

    if (vk.device) vkDestroyDevice(vk.device, nullptr);
    if (vk.surface) vkDestroySurfaceKHR(vk.instance, vk.surface, nullptr);
    
    // Destroy debug messenger before destroying instance
    destroyDebugMessenger(vk.instance, vk.debugMessenger);
    
    if (vk.instance) vkDestroyInstance(vk.instance, nullptr);

    SDL_DestroyWindow(window);
    SDL_Quit();

    spdlog::info("Luster sandbox exiting.");
    return EXIT_SUCCESS;
}