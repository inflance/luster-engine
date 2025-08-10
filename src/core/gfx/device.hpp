#pragma once

#include "core/core.hpp"

namespace luster::gfx
{
	class Device
	{
	public:
		Device();
		~Device();

		void init(SDL_Window* window);
		void cleanup();
		void waitIdle() const;

		// Accessors
		VkInstance instance() const { return instance_; }
		VkSurfaceKHR surface() const { return surface_; }
		VkPhysicalDevice physical() const { return gpu_; }
		VkDevice logical() const { return device_; }
		uint32_t gfxQueueFamily() const { return gfxQueueFamily_; }
		uint32_t presentQueueFamily() const { return presentQueueFamily_; }
		VkQueue gfxQueue() const { return gfxQueue_; }
		VkQueue presentQueue() const { return presentQueue_; }

	private:
		void createInstance(SDL_Window* window);
		void pickDevice();
		void createDevice();
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
	};
}
