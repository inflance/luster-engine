#pragma once

#include "core/core.hpp"
#include <vector>

// forward decls
namespace luster { class Window; }
namespace luster::gfx { class Device; class Swapchain; class RenderPass; class Pipeline; class CommandContext; class Framebuffers; }

namespace luster
{
	class Renderer
	{
	public:
		Renderer();
		~Renderer();

		void init(SDL_Window* window);
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

		void createInstance(SDL_Window* window);
		void pickDevice();
		void createDevice();
		void createSwapchainAndViews(SDL_Window* window);
        void createRenderPass();
        void createPipeline();
		void createFramebuffers();
		void createCommandsAndSync();
		void cleanupSwapchain();
		void recordAndSubmitFrame(uint32_t imageIndex);
	};
} // namespace luster