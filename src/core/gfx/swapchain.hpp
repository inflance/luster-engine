#pragma once

#include "core/core.hpp"

namespace luster::gfx
{
	class Device;

	struct SwapchainCreateInfo
	{
		VkFormat preferredFormat = VK_FORMAT_B8G8R8A8_UNORM;
		VkColorSpaceKHR preferredColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		// FIFO 可锁帧到显示器刷新率；MAILBOX 可低延迟（若可用）
		VkPresentModeKHR preferredPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	};

	class Swapchain
	{
	public:
		Swapchain() = default;
		~Swapchain() = default;

		void create(const Device& device, SDL_Window* window, const SwapchainCreateInfo& info = {});
		void recreate(const Device& device, SDL_Window* window, const SwapchainCreateInfo& info = {});
		void cleanup(const Device& device);

		// Accessors
		VkSwapchainKHR handle() const { return swapchain_; }
		VkFormat imageFormat() const { return swapFormat_; }
		VkExtent2D extent() const { return swapExtent_; }
		const std::vector<VkImageView>& imageViews() const { return swapImageViews_; }

	private:
		static VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats,
		                                              VkFormat preferredFormat,
		                                              VkColorSpaceKHR preferredColorSpace);
		static VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes);
		static VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& caps, SDL_Window* window);

		VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
		VkFormat swapFormat_ = VK_FORMAT_B8G8R8A8_UNORM;
		VkColorSpaceKHR swapColorSpace_ = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		VkExtent2D swapExtent_{};
		std::vector<VkImage> swapImages_;
		std::vector<VkImageView> swapImageViews_;
	};
}
