#pragma once

#include "core/core.hpp"
#include "core/gfx/device.hpp"
#include "core/gfx/gpu_profiler.hpp"
#include "core/utils/profiler.hpp"
#include "core/utils/fps_counter.hpp"
#include "core/config.hpp"
#include <chrono>
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
      class Image;
      class Buffer;
	}

	class Renderer
	{
	public:
		Renderer();
		~Renderer();

		void init(SDL_Window* window, const EngineConfig& config);
		// Backward-compatible overload: only device params → build EngineConfig under the hood
		void init(SDL_Window* window, const gfx::Device::InitParams& params = gfx::Device::InitParams{});
		bool drawFrame(SDL_Window* window);
		void recreateSwapchain(SDL_Window* window);
		void cleanup();

	private:
		EngineConfig config_{};
		// Core render state
		std::unique_ptr<gfx::Device> device_;
		std::unique_ptr<gfx::Swapchain> swapchain_;

		std::unique_ptr<gfx::RenderPass> renderPass_;
		std::unique_ptr<gfx::Pipeline> pipeline_;
		std::unique_ptr<gfx::Framebuffers> framebuffers_;
          std::unique_ptr<gfx::Image> depthImage_;

		std::unique_ptr<gfx::CommandContext> context_;
        gfx::GpuProfiler gpuProfiler_;

        // Geometry & UBO
        std::unique_ptr<gfx::Buffer> vertexBuffer_;
        std::unique_ptr<gfx::Buffer> uniformBuffer_;
        VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;

        // FPS tracking
        std::chrono::steady_clock::time_point fpsLastUpdate_{};
        double fpsAccumMs_ = 0.0;
        int fpsCount_ = 0;
        // CPU侧 FPS 统计
        luster::FpsCounter cpuFps_{"CPU"};
        luster::FpsCounter gpuFps_{"GPU"};

		void createInstance(SDL_Window* window, const gfx::Device::InitParams& params);
		void createSwapchainAndViews(SDL_Window* window);
		void createRenderPass();
		void createPipeline();
		void createFramebuffers();
		void createCommandsAndSync();
        void createGeometry();
        void createDescriptors();
		void cleanupSwapchain();
	};
} // namespace luster
