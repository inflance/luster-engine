#include "core/renderer.hpp"
#include "core/gfx/device.hpp"
#include "core/gfx/swapchain.hpp"
#include "core/gfx/shader.hpp"
#include "core/gfx/render_pass.hpp"
#include "core/gfx/pipeline.hpp"
#include "core/gfx/command_context.hpp"
#include "core/gfx/framebuffers.hpp"
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

namespace luster
{
	// SDL error logging helper
    static void log_sdl_error(const char* what)
	{
		const char* err = SDL_GetError();
		spdlog::error("{}: {}", what, (err && *err) ? err : "unknown");
	}

	static const char* vk_err(VkResult r)
	{
		switch (r)
		{
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
	              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	              void* pUserData)
	{
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
		if (vkEnumerateInstanceLayerProperties(&count, nullptr) != VK_SUCCESS)
			return false;
		std::vector<VkLayerProperties> layers(count);
		if (count)
			vkEnumerateInstanceLayerProperties(&count, layers.data());
		for (auto& lp : layers)
			if (std::strcmp(lp.layerName, name) == 0)
				return true;
		return false;
	}

	static bool hasInstanceExtension(const char* name)
	{
		uint32_t count = 0;
		if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) !=
			VK_SUCCESS)
			return false;
		std::vector<VkExtensionProperties> exts(count);
		if (count)
			vkEnumerateInstanceExtensionProperties(nullptr, &count, exts.data());
		for (auto& ep : exts)
			if (std::strcmp(ep.extensionName, name) == 0)
				return true;
		return false;
	}

	static VkResult setupDebugMessenger(VkInstance instance,
	                                    VkDebugUtilsMessengerEXT* out)
	{
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
	                                  VkDebugUtilsMessengerEXT msgr)
	{
		if (!msgr)
			return;
		auto fpDestroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
		if (fpDestroy)
			fpDestroy(instance, msgr, nullptr);
	}

	struct QueueFamilies
	{
		std::optional<uint32_t> graphics;
		std::optional<uint32_t> present;
		bool complete() const { return graphics.has_value() && present.has_value(); }
	};

	struct SwapchainSupport
	{
		VkSurfaceCapabilitiesKHR caps{};
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	static std::vector<char> readFileBinary(const std::string& path)
	{
		std::ifstream f(path, std::ios::ate | std::ios::binary);
		if (!f)
			throw std::runtime_error("Failed to open file: " + path);
		const size_t size = f.tellg();
		std::vector<char> buffer(size);
		f.seekg(0);
		if (!f.read(buffer.data(), buffer.size()))
		{
			throw std::runtime_error("Failed to read file: " + path);
		}
		return buffer;
	}

	static QueueFamilies findQueueFamilies(VkPhysicalDevice gpu,
	                                       VkSurfaceKHR surface)
	{
		QueueFamilies out{};
		uint32_t count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, nullptr);
		std::vector<VkQueueFamilyProperties> props(count);
		vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, props.data());
		for (uint32_t i = 0; i < count; ++i)
		{
			if (!out.graphics && (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
			{
				out.graphics = i;
			}
			if (!out.present)
			{
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
	                                              VkSurfaceKHR surface)
	{
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
	chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
	{
		if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
		{
			return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
		}
		for (auto& f : formats)
		{
			if ((f.format == VK_FORMAT_B8G8R8A8_SRGB ||
					f.format == VK_FORMAT_B8G8R8A8_UNORM) &&
				f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return f;
			}
		}
		return formats[0];
	}

	static VkPresentModeKHR
	choosePresentMode(const std::vector<VkPresentModeKHR>& modes)
	{
		for (auto m : modes)
		{
			if (m == VK_PRESENT_MODE_MAILBOX_KHR)
				return m;
		}
		return VK_PRESENT_MODE_FIFO_KHR; // guaranteed
	}

	static VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& caps,
	                               SDL_Window* window)
	{
		if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
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
	                                         const std::vector<char>& code)
	{
		VkShaderModuleCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		ci.codeSize = code.size();
		ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
		VkShaderModule mod = VK_NULL_HANDLE;
		VkResult r = vkCreateShaderModule(device, &ci, nullptr, &mod);
		if (r != VK_SUCCESS)
			throw std::runtime_error(std::string("vkCreateShaderModule failed: ") +
				vk_err(r));
		return mod;
	}

    Renderer::Renderer() = default;
    Renderer::~Renderer() { cleanup(); }

    void Renderer::init(SDL_Window* window)
	{
		try
		{
            // Create device (includes instance, surface, physical & logical device)
            createInstance(window);
            spdlog::info("Device initialized");

            // Create swapchain
            createSwapchainAndViews(window);
            spdlog::info("Swapchain created: {} images, {}x{}, format {}",
                         static_cast<int>(swapchain_->imageViews().size()),
                         swapchain_->extent().width, swapchain_->extent().height,
                         static_cast<int>(swapchain_->imageFormat()));

        createRenderPass();
        createPipeline();
            createFramebuffers();
            createCommandsAndSync();
		}
		catch (const std::exception& ex)
		{
			spdlog::critical("Vulkan initialization failed: {}", ex.what());
			throw;
		}
	}

    bool Renderer::drawFrame(SDL_Window* window)
	{
        // 等待/复位栅栏改用 CommandContext 封装
        context_->waitFence(*device_, UINT64_C(1'000'000'000));
        VkResult r = VK_SUCCESS;
		if (r != VK_SUCCESS)
		{
			spdlog::error("vkWaitForFences failed: {}", vk_err(r));
			return false;
		}
        context_->resetFence(*device_);

		uint32_t imageIndex = 0;
        r = vkAcquireNextImageKHR(device_->logical(), swapchain_->handle(), UINT64_MAX,
                                  context_->imageAvailable(), VK_NULL_HANDLE, &imageIndex);
		if (r == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreateSwapchain(window);
			return true;
		}
		if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR)
		{
			spdlog::error("vkAcquireNextImageKHR failed: {}", vk_err(r));
			return false;
		}

		try
		{
			recordAndSubmitFrame(imageIndex);
		}
		catch (const std::exception& ex)
		{
			spdlog::error("record/submit failed: {}", ex.what());
			return false;
		}

        VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
		pi.waitSemaphoreCount = 1;
        VkSemaphore renderFinished = context_->renderFinished();
        pi.pWaitSemaphores = &renderFinished;
		pi.swapchainCount = 1;
        VkSwapchainKHR sc = swapchain_->handle();
        pi.pSwapchains = &sc;
		pi.pImageIndices = &imageIndex;
        r = vkQueuePresentKHR(device_->presentQueue(), &pi);
		if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR)
		{
			recreateSwapchain(window);
		}
		else if (r != VK_SUCCESS)
		{
			spdlog::error("vkQueuePresentKHR failed: {}", vk_err(r));
			return false;
		}

		return true;
	}

    void Renderer::recreateSwapchain(SDL_Window* window)
	{
		int w = 0, h = 0;
		SDL_GetWindowSize(window, &w, &h);
		if (w == 0 || h == 0)
			return;

        device_->waitIdle();
        cleanupSwapchain();
        swapchain_->recreate(*device_, window);
        createRenderPass();
        createPipeline();
        createFramebuffers();
	}

    void Renderer::cleanup()
	{
        if (!device_)
            return;

        device_->waitIdle();

        if (context_)
        {
            context_->cleanup(*device_);
            context_.reset();
        }

        cleanupSwapchain();

        if (swapchain_)
        {
            swapchain_->cleanup(*device_);
            swapchain_.reset();
        }
        if (device_)
        {
            device_->cleanup();
            device_.reset();
        }
	}

    void Renderer::createInstance(SDL_Window* window)
    {
        device_ = std::make_unique<gfx::Device>();
        device_->init(window);
    }

    void Renderer::pickDevice() { /* handled in Device::init */ }

    void Renderer::createDevice() { /* handled in Device::init */ }

    void Renderer::createSwapchainAndViews(SDL_Window* window)
    {
        swapchain_ = std::make_unique<gfx::Swapchain>();
        swapchain_->create(*device_, window);
    }

    void Renderer::createRenderPass()
    {
        renderPass_ = std::make_unique<gfx::RenderPass>();
        renderPass_->create(*device_, swapchain_->imageFormat());
    }

    void Renderer::createPipeline()
    {
        pipeline_ = std::make_unique<gfx::Pipeline>();
        gfx::PipelineCreateInfo info{};
        info.vsSpvPath = "shaders/triangle.vert.spv";
        info.fsSpvPath = "shaders/triangle.frag.spv";
        info.viewportExtent = swapchain_->extent();
        pipeline_->create(*device_, *renderPass_, info);
    }

    void Renderer::createFramebuffers()
	{
        if (!framebuffers_) framebuffers_ = std::make_unique<gfx::Framebuffers>();
        framebuffers_->create(*device_, *renderPass_, swapchain_->extent(), swapchain_->imageViews());
	}

    void Renderer::createCommandsAndSync()
	{
        if (!context_) context_ = std::make_unique<gfx::CommandContext>();
        context_->create(*device_, device_->gfxQueueFamily());
        context_->createSync(*device_);
	}

    void Renderer::cleanupSwapchain()
	{
        if (framebuffers_)
        {
            framebuffers_->cleanup(*device_);
            framebuffers_.reset();
        }

        if (pipeline_)
        {
            pipeline_->cleanup(*device_);
            pipeline_.reset();
        }
        if (renderPass_)
        {
            renderPass_->cleanup(*device_);
            renderPass_.reset();
        }
	}

    void Renderer::recordAndSubmitFrame(uint32_t imageIndex)
	{
        VkCommandBuffer cbBegin = context_->begin();

        context_->beginRender(*renderPass_, framebuffers_->handles()[imageIndex], swapchain_->extent(),
                              0.05f, 0.06f, 0.09f, 1.0f);
        context_->bindPipeline(*pipeline_);
        context_->draw(3);
        context_->endRender();

        context_->end();

        context_->submit(device_->gfxQueue(), context_->imageAvailable(), context_->renderFinished(), context_->inFlight());
	}
} // namespace luster
