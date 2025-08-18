#pragma once

#include "core/core.hpp"
#include <vector>
#include <functional>

namespace luster { class Window; }

namespace luster::gfx
{
	class Device
	{
	public:
		struct InitParams
		{
			bool enableValidation = true;
			bool enableDebugUtils = true;
			std::vector<const char*> extraInstanceExtensions{};
			std::vector<const char*> extraInstanceLayers{};
			std::vector<const char*> extraDeviceExtensions{};
		};
		Device();
		~Device();

		void init(::luster::Window& window, const InitParams& params = InitParams{});
		void cleanup();
		void waitIdle() const;
		bool isInitialized() const { return device_ != VK_NULL_HANDLE; }

		// Accessors
		VkInstance instance() const { return instance_; }
		VkSurfaceKHR surface() const { return surface_; }
		VkPhysicalDevice physical() const { return gpu_; }
		VkDevice logical() const { return device_; }
		uint32_t gfxQueueFamily() const { return gfxQueueFamily_; }
		uint32_t presentQueueFamily() const { return presentQueueFamily_; }
		VkQueue gfxQueue() const { return gfxQueue_; }
		VkQueue presentQueue() const { return presentQueue_; }
		float timestampPeriod() const { return timestampPeriod_; }

		// Helpers
		VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
		VkFormat findDepthFormat() const; // depth-only preferred
        void submitImmediate(const std::function<void(VkCommandBuffer)>& recordCommands) const;

	private:
		void createInstance(::luster::Window& window, const InitParams& params);
		void pickDevice();
		void createDevice(const InitParams& params);
		void destroyDebugMessenger();

		// Vulkan objects
		VkInstance instance_ = VK_NULL_HANDLE;
		VkSurfaceKHR surface_ = VK_NULL_HANDLE;
		VkPhysicalDevice gpu_ = VK_NULL_HANDLE;
		VkDevice device_ = VK_NULL_HANDLE;

		uint32_t gfxQueueFamily_ = 0;
		uint32_t presentQueueFamily_ = 0;
		VkQueue gfxQueue_ = VK_NULL_HANDLE;
		VkQueue presentQueue_ = VK_NULL_HANDLE;

		VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
		float timestampPeriod_ = 0.0f;
	};
}
