#pragma once

#include "core/core.hpp"
#include "core/gfx/device.hpp"
#include <vector>

namespace luster
{
	class Window;

	namespace gfx
	{
		class Swapchain;
		class RenderPass;
		class Pipeline;
		class CommandContext;
		class Framebuffers;
	}

	class Renderer
	{
	public:
		Renderer();
		~Renderer();

		void init(SDL_Window* window, const gfx::Device::InitParams& params = gfx::Device::InitParams{});
		bool drawFrame(SDL_Window* window);
		void recreateSwapchain(SDL_Window* window);
		void cleanup();

	private:
		// Core render state
		std::unique_ptr<gfx::Device> device_;
		std::unique_ptr<gfx::Swapchain> swapchain_;

		std::unique_ptr<gfx::RenderPass> renderPass_;
		std::unique_ptr<gfx::Pipeline> pipeline_;
		std::unique_ptr<gfx::Framebuffers> framebuffers_;

		std::unique_ptr<gfx::CommandContext> context_;

		void createInstance(SDL_Window* window, const gfx::Device::InitParams& params);
		void createSwapchainAndViews(SDL_Window* window);
		void createRenderPass();
		void createPipeline();
		void createFramebuffers();
		void createCommandsAndSync();
		void cleanupSwapchain();
	};
} // namespace luster
