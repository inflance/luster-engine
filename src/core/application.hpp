// Copyright (c) Luster
#pragma once

#include "core/core.hpp"
#include "core/window.hpp"

namespace luster
{
	class Application
	{
	public:
		Application();
		~Application();

		void run();

	private:
		void init();
		void mainLoop();
		void cleanup();

		struct VulkanState;
		VulkanState* vk_state_ = nullptr;
		Window* window_ = nullptr;

		// Vulkan initialization helper methods
		void createInstance(SDL_Window* window, VulkanState& vk);
		void pickDevice(VulkanState& vk);
		void createDevice(VulkanState& vk);
		void createSwapchainAndViews(SDL_Window* window, VulkanState& vk);
		void createRenderPass(VulkanState& vk);
		void createPipeline(VulkanState& vk);
		void createFramebuffers(VulkanState& vk);
		void createCommandsAndSync(VulkanState& vk);
		void cleanupSwapchain(VulkanState& vk);
		void recreateSwapchain(SDL_Window* window, VulkanState& vk);
		void recordAndSubmitFrame(VulkanState& vk, uint32_t imageIndex);
	};
} // namespace luster
