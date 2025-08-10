#include "core/gfx/device.hpp"
#include <vector>
#include <optional>
#include <cstring>
#include <stdexcept>
#include <array>
#include <algorithm>

namespace luster::gfx
{
	// Helpers duplicated locally (keep internal linkage)
	static const char* vk_err(VkResult r)
	{
		switch (r)
		{
		case VK_SUCCESS: return "VK_SUCCESS";
		case VK_NOT_READY: return "VK_NOT_READY";
		case VK_TIMEOUT: return "VK_TIMEOUT";
		case VK_EVENT_SET: return "VK_EVENT_SET";
		case VK_EVENT_RESET: return "VK_EVENT_RESET";
		case VK_INCOMPLETE: return "VK_INCOMPLETE";
		case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
		case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
		case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
		case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
		case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
		case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
		case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
		case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
		case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
		case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
		case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
		case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
		case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
		case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
		case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
		case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
		case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
		default: return "VK_UNKNOWN";
		}
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageTypes,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		(void)messageTypes;
		(void)pUserData;
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
			spdlog::error("[Vulkan] {}", pCallbackData ? pCallbackData->pMessage : "(null)");
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
			spdlog::warn("[Vulkan] {}", pCallbackData ? pCallbackData->pMessage : "(null)");
		else
			spdlog::info("[Vulkan] {}", pCallbackData ? pCallbackData->pMessage : "(null)");
		return VK_FALSE;
	}

	static void fillDebugCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& info)
	{
		info = {};
		info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
		info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		info.pfnUserCallback = debugCallback;
	}

	static bool hasInstanceLayer(const char* name)
	{
		uint32_t count = 0;
		if (vkEnumerateInstanceLayerProperties(&count, nullptr) != VK_SUCCESS) return false;
		std::vector<VkLayerProperties> layers(count);
		if (count) vkEnumerateInstanceLayerProperties(&count, layers.data());
		for (auto& lp : layers) if (std::strcmp(lp.layerName, name) == 0) return true;
		return false;
	}

	static bool hasInstanceExtension(const char* name)
	{
		uint32_t count = 0;
		if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS) return false;
		std::vector<VkExtensionProperties> exts(count);
		if (count) vkEnumerateInstanceExtensionProperties(nullptr, &count, exts.data());
		for (auto& ep : exts) if (std::strcmp(ep.extensionName, name) == 0) return true;
		return false;
	}

	static VkResult setupDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT* out)
	{
		if (!out) return VK_ERROR_INITIALIZATION_FAILED;
		auto fpCreate = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
		if (!fpCreate) return VK_ERROR_EXTENSION_NOT_PRESENT;
		VkDebugUtilsMessengerCreateInfoEXT info{};
		fillDebugCreateInfo(info);
		return fpCreate(instance, &info, nullptr, out);
	}

	static void destroyDebugMessengerExt(VkInstance instance, VkDebugUtilsMessengerEXT msgr)
	{
		if (!msgr) return;
		auto fpDestroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
		if (fpDestroy) fpDestroy(instance, msgr, nullptr);
	}

	Device::Device() = default;
	Device::~Device() { cleanup(); }

    void Device::init(SDL_Window* window, const InitParams& params)
	{
        createInstance(window, params);
        pickDevice();
        createDevice(params);
	}

	void Device::cleanup()
	{
		if (!instance_) return;
		if (device_) vkDeviceWaitIdle(device_);

		if (device_) vkDestroyDevice(device_, nullptr);
		if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
		destroyDebugMessenger();
		if (instance_) vkDestroyInstance(instance_, nullptr);

		instance_ = VK_NULL_HANDLE;
		surface_ = VK_NULL_HANDLE;
		gpu_ = VK_NULL_HANDLE;
		device_ = VK_NULL_HANDLE;
		gfxQueue_ = VK_NULL_HANDLE;
		presentQueue_ = VK_NULL_HANDLE;
		gfxQueueFamily_ = 0;
		presentQueueFamily_ = 0;
	}

	void Device::waitIdle() const
	{
		if (device_) vkDeviceWaitIdle(device_);
	}

    void Device::createInstance(SDL_Window* window, const InitParams& params)
	{
		Uint32 extCount = 0;
		const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);
		if (!sdlExts || extCount == 0)
		{
			throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions returned empty");
		}
		std::vector<const char*> extensions(sdlExts, sdlExts + extCount);
        // layers
        std::vector<const char*> layers;
        if (params.enableValidation && hasInstanceLayer("VK_LAYER_KHRONOS_validation"))
        {
            layers.push_back("VK_LAYER_KHRONOS_validation");
            spdlog::info("Enabling validation layer: VK_LAYER_KHRONOS_validation");
        }
        for (auto* l : params.extraInstanceLayers) layers.push_back(l);

        // extensions
        bool canDebugUtils = hasInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        if (params.enableDebugUtils && canDebugUtils)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            spdlog::info("Enabling instance extension: {}", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        else if (params.enableDebugUtils)
        {
            spdlog::warn("Instance extension not available: {}", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        for (auto* e : params.extraInstanceExtensions) extensions.push_back(e);

		VkApplicationInfo app{};
		app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app.pApplicationName = "Luster";
		app.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
		app.pEngineName = "Luster";
		app.engineVersion = VK_MAKE_VERSION(0, 1, 0);
		app.apiVersion = VK_API_VERSION_1_3;

        VkDebugUtilsMessengerCreateInfoEXT dbgInfo{};
        if (params.enableDebugUtils && canDebugUtils) fillDebugCreateInfo(dbgInfo);

		VkInstanceCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		ci.pApplicationInfo = &app;
		ci.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		ci.ppEnabledExtensionNames = extensions.data();
		ci.enabledLayerCount = static_cast<uint32_t>(layers.size());
		ci.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();
        ci.pNext = (params.enableDebugUtils && canDebugUtils) ? &dbgInfo : nullptr;

		VkResult r = vkCreateInstance(&ci, nullptr, &instance_);
		if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkCreateInstance failed: ") + vk_err(r));

        if (params.enableDebugUtils && canDebugUtils)
		{
			if (setupDebugMessenger(instance_, &debugMessenger_) != VK_SUCCESS)
			{
				spdlog::warn("Failed to create debug messenger");
			}
		}

		if (!SDL_Vulkan_CreateSurface(window, instance_, nullptr, &surface_) || surface_ == VK_NULL_HANDLE)
		{
			throw std::runtime_error("SDL_Vulkan_CreateSurface failed");
		}
	}

	struct QueueFamilies
	{
		std::optional<uint32_t> graphics;
		std::optional<uint32_t> present;
	};

	static QueueFamilies findQueueFamilies(VkPhysicalDevice gpu, VkSurfaceKHR surface)
	{
		QueueFamilies out{};
		uint32_t count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, nullptr);
		std::vector<VkQueueFamilyProperties> props(count);
		vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, props.data());
		for (uint32_t i = 0; i < count; ++i)
		{
			if (!out.graphics && (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) out.graphics = i;
			if (!out.present)
			{
				VkBool32 presentSupport = VK_FALSE;
				vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &presentSupport);
				if (presentSupport) out.present = i;
			}
			if (out.graphics && out.present) break;
		}
		return out;
	}

	void Device::pickDevice()
	{
		uint32_t count = 0;
		vkEnumeratePhysicalDevices(instance_, &count, nullptr);
		if (!count) throw std::runtime_error("No Vulkan physical devices");
		std::vector<VkPhysicalDevice> gpus(count);
		vkEnumeratePhysicalDevices(instance_, &count, gpus.data());
		for (auto d : gpus)
		{
			QueueFamilies q = findQueueFamilies(d, surface_);
			uint32_t extCount = 0;
			vkEnumerateDeviceExtensionProperties(d, nullptr, &extCount, nullptr);
			std::vector<VkExtensionProperties> devExts(extCount);
			vkEnumerateDeviceExtensionProperties(d, nullptr, &extCount, devExts.data());
			bool hasSwapchain = false;
			for (auto& e : devExts) if (std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) hasSwapchain
				= true;
			if (q.graphics && q.present && hasSwapchain)
			{
				gpu_ = d;
				gfxQueueFamily_ = q.graphics.value();
				presentQueueFamily_ = q.present.value();
				return;
			}
		}
		throw std::runtime_error("Failed to find suitable GPU");
	}

    void Device::createDevice(const InitParams& params)
	{
		float prio = 1.0f;
		std::vector<VkDeviceQueueCreateInfo> qcis;
		std::array<uint32_t, 2> unique = {gfxQueueFamily_, presentQueueFamily_};
		std::sort(unique.begin(), unique.end());
		auto last = std::unique(unique.begin(), unique.end());
		for (auto it = unique.begin(); it != last; ++it)
		{
			VkDeviceQueueCreateInfo qci{};
			qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			qci.queueFamilyIndex = *it;
			qci.queueCount = 1;
			qci.pQueuePriorities = &prio;
			qcis.push_back(qci);
		}

		VkPhysicalDeviceFeatures feats{};
        std::vector<const char*> extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        for (auto* e : params.extraDeviceExtensions) extensions.push_back(e);

		VkDeviceCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		ci.queueCreateInfoCount = static_cast<uint32_t>(qcis.size());
		ci.pQueueCreateInfos = qcis.data();
		ci.pEnabledFeatures = &feats;
        ci.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        ci.ppEnabledExtensionNames = extensions.data();

		VkResult r = vkCreateDevice(gpu_, &ci, nullptr, &device_);
		if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkCreateDevice failed: ") + vk_err(r));

		vkGetDeviceQueue(device_, gfxQueueFamily_, 0, &gfxQueue_);
		vkGetDeviceQueue(device_, presentQueueFamily_, 0, &presentQueue_);
	}

	void Device::destroyDebugMessenger()
	{
		if (debugMessenger_) destroyDebugMessengerExt(instance_, debugMessenger_);
		debugMessenger_ = VK_NULL_HANDLE;
	}
}
