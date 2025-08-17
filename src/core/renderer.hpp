#pragma once

#include "core/core.hpp"
#include "core/gfx/device.hpp"
#include "core/gfx/gpu_profiler.hpp"
#include "core/utils/profiler.hpp"
#include "core/utils/fps_counter.hpp"
#include "core/config.hpp"
#include "core/camera.hpp"
#include "core/camera_controller.hpp"
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
      class VertexLayout;
      class DescriptorSetLayout;
      class DescriptorPool;
      class DescriptorSet;
      class Mesh;
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
		void update(float dt);
		void update(float dt, const InputSnapshot& input);
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
        std::unique_ptr<gfx::Buffer> indexBuffer_;
        std::unique_ptr<gfx::Mesh> mesh_;
        std::unique_ptr<gfx::Buffer> uniformBuffer_;
        std::unique_ptr<gfx::VertexLayout> vertexLayout_;
        std::unique_ptr<gfx::DescriptorSetLayout> dsl_;
        std::unique_ptr<gfx::DescriptorPool> dsp_;
        std::unique_ptr<gfx::DescriptorSet> dset_;

        // FPS tracking
        std::chrono::steady_clock::time_point fpsLastUpdate_{};
        double fpsAccumMs_ = 0.0;
        int fpsCount_ = 0;
        // CPU侧 FPS 统计
        luster::FpsCounter cpuFps_{"CPU"};
        luster::FpsCounter gpuFps_{"GPU"};

        // Camera
        Camera camera_{};
        CameraController cameraController_{};
        std::chrono::steady_clock::time_point camLogLast_{};
        double camLogIntervalMs_ = 500.0;

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
