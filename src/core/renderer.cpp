#include "core/renderer.hpp"
#include "core/gfx/device.hpp"
#include "core/gfx/swapchain.hpp"
#include "core/gfx/shader.hpp"
#include "core/gfx/render_pass.hpp"
#include "core/gfx/pipeline.hpp"
#include "core/gfx/command_context.hpp"
#include "core/gfx/framebuffers.hpp"
#include <cstdint>
#include <stdexcept>

namespace luster
{
	// 旧的静态辅助函数均已下沉到 gfx 层或不再需要，移除以减少编译警告

	Renderer::Renderer() = default;
	Renderer::~Renderer() { cleanup(); }

	void Renderer::init(SDL_Window* window, const gfx::Device::InitParams& params)
	{
		try
		{
			// Create device (includes instance, surface, physical & logical device)
			createInstance(window, params);
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
            gpuProfiler_.init(*device_);

            auto now0 = std::chrono::steady_clock::now();
            fpsLastUpdate_ = now0;
            fpsAccumMs_ = 0.0;
            fpsCount_ = 0;
		}
		catch (const std::exception& ex)
		{
			spdlog::critical("Vulkan initialization failed: {}", ex.what());
			throw;
		}
	}

	bool Renderer::drawFrame(SDL_Window* window)
	{
        auto result = framebuffers_->drawFrame(
			window, *device_, *renderPass_, *context_, *swapchain_,
			[&](VkCommandBuffer /*cb*/, uint32_t imageIndex)
			{
                gpuProfiler_.beginLabel(*context_, "TrianglePass");
                gpuProfiler_.beginFrame(*context_);
                context_->beginRender(*renderPass_, framebuffers_->handles()[imageIndex], swapchain_->extent(),
				                      0.05f, 0.06f, 0.09f, 1.0f);
				context_->bindPipeline(*pipeline_);
				context_->draw(3);
				context_->endRender();
                gpuProfiler_.endFrame(*context_);
                gpuProfiler_.endLabel(*context_);
            }
		);

        if (result == gfx::Framebuffers::FrameResult::NeedRecreate)
        {
            recreateSwapchain(window);
            return true;
        }
		if (result == gfx::Framebuffers::FrameResult::Error)
		{
			spdlog::error("drawFrame failed");
			return false;
		}

        // FPS accumulation based on GPU frame time
        // GPU-based FPS
        double ms = 0.0;
        if (gpuProfiler_.getLastTimingMs(*device_, ms))
        {
            fpsAccumMs_ += ms;
            ++fpsCount_;
            gpuFps_.addSampleMs(ms);
        }

        // CPU-based FPS（基于每帧调用频率估算，不反映GPU时）
        cpuFps_.tick();

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
		gfx::SwapchainCreateInfo sci{};
		sci.preferredPresentMode = VK_PRESENT_MODE_FIFO_KHR;
		swapchain_->recreate(*device_, window, sci);
		createRenderPass();
		createPipeline();
		createFramebuffers();
	}

	void Renderer::cleanup()
	{
		if (!device_)
			return;

		device_->waitIdle();

        // Destroy GPU profiler resources before device is destroyed
        gpuProfiler_.cleanup(*device_);

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

	void Renderer::createInstance(SDL_Window* window, const gfx::Device::InitParams& params)
	{
		device_ = std::make_unique<gfx::Device>();
		device_->init(window, params);
	}

	void Renderer::createSwapchainAndViews(SDL_Window* window)
	{
		swapchain_ = std::make_unique<gfx::Swapchain>();
		gfx::SwapchainCreateInfo sci{};
		sci.preferredPresentMode = VK_PRESENT_MODE_FIFO_KHR; // 默认 VSync，可配置
		swapchain_->create(*device_, window, sci);
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
} // namespace luster
