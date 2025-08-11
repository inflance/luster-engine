#include "core/renderer.hpp"
#include "core/gfx/device.hpp"
#include "core/gfx/swapchain.hpp"
#include "core/gfx/shader.hpp"
#include "core/gfx/render_pass.hpp"
#include "core/gfx/pipeline.hpp"
#include "core/gfx/command_context.hpp"
#include "core/gfx/framebuffers.hpp"
#include "core/gfx/image.hpp"
#include "core/gfx/buffer.hpp"
#include "core/gfx/vertex_layout.hpp"
#include "core/gfx/descriptor.hpp"
#include "core/gfx/mesh.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "utils/log.hpp"

namespace luster
{
	// 旧的静态辅助函数均已下沉到 gfx 层或不再需要，移除以减少编译警告

	Renderer::Renderer() = default;
	Renderer::~Renderer() { cleanup(); }

	void Renderer::init(SDL_Window* window, const EngineConfig& config)
	{
		try
		{
			// Cache config
			config_ = config;
			// Create device (includes instance, surface, physical & logical device)
			createInstance(window, config_.device);
			spdlog::info("Device initialized");

			// Create swapchain
			createSwapchainAndViews(window);
			spdlog::info("Swapchain created: {} images, {}x{}, format {}",
			             static_cast<int>(swapchain_->imageViews().size()),
			             swapchain_->extent().width, swapchain_->extent().height,
			             static_cast<int>(swapchain_->imageFormat()));

			createRenderPass();
			createFramebuffers();
			createGeometry();
			createDescriptors();
			createCommandsAndSync();
			gpuProfiler_.init(*device_);

			auto now0 = std::chrono::steady_clock::now();
			fpsLastUpdate_ = now0;
			fpsAccumMs_ = 0.0;
			fpsCount_ = 0;
			// Apply FPS intervals if customized
			cpuFps_.setReportIntervalMs(config_.fpsReportIntervalMs);
			gpuFps_.setReportIntervalMs(config_.fpsReportIntervalMs);
		}
		catch (const std::exception& ex)
		{
			spdlog::critical("Vulkan initialization failed: {}", ex.what());
			throw;
		}
	}

	void Renderer::init(SDL_Window* window, const gfx::Device::InitParams& params)
	{
		EngineConfig cfg{};
		cfg.device = params;
		cfg.swapchain.preferredPresentMode = VK_PRESENT_MODE_FIFO_KHR;
		cfg.fpsReportIntervalMs = 500.0;
		init(window, cfg);
	}

	void Renderer::update(float dt)
	{
		// 更新相机（使用真实 dt）
		camera_.setPerspective(glm::radians(60.0f),
		                       static_cast<float>(swapchain_->extent().width) / static_cast<float>(swapchain_->extent().
			                       height), 0.1f, 100.0f);
		camera_.updateFromSdl(dt);
		// 定期打印相机位置
		const auto now = std::chrono::steady_clock::now();
		if (camLogLast_.time_since_epoch().count() == 0) camLogLast_ = now;
		const double elapsedMs = static_cast<double>(
			std::chrono::duration_cast<std::chrono::milliseconds>(now - camLogLast_).count());
		if (elapsedMs >= camLogIntervalMs_)
		{
			const bool* ks = SDL_GetKeyboardState(nullptr);
			bool any = ks[SDL_SCANCODE_W] || ks[SDL_SCANCODE_A] || ks[SDL_SCANCODE_S] || ks[SDL_SCANCODE_D];
			const glm::vec3& e = camera_.eye();
			if (any)
				spdlog::info("Camera pos: ({:.3f}, {:.3f}, {:.3f}) WASD:{}{}{}{} dt:{:.3f}", e.x, e.y, e.z,
				             ks[SDL_SCANCODE_W] ? "W" : "-", ks[SDL_SCANCODE_A] ? "A" : "-",
				             ks[SDL_SCANCODE_S] ? "S" : "-", ks[SDL_SCANCODE_D] ? "D" : "-", dt);
			else
				spdlog::info("Camera pos: ({:.3f}, {:.3f}, {:.3f})", e.x, e.y, e.z);
			camLogLast_ = now;
		}
	}

	bool Renderer::drawFrame(SDL_Window* window)
	{
		// Update MVP (rotation over time)
		static auto t0 = std::chrono::steady_clock::now();
		auto now = std::chrono::steady_clock::now();
		float seconds = std::chrono::duration<float>(now - t0).count();
		const glm::mat4& proj = camera_.proj();
		const glm::mat4& view = glm::translate(glm::mat4(1.0f), {0, 0, -1});
		glm::mat4 model = glm::rotate(glm::mat4(1.0f), seconds, glm::vec3(0, 1, 0));
		glm::mat4 mvp = proj * view * model;
		if (uniformBuffer_)
		{
			void* p = uniformBuffer_->map(*device_);
			memcpy(p, &mvp, sizeof(mvp));
			uniformBuffer_->unmap(*device_);
		}
		else
		{
			spdlog::error("uniform buffer not init");
		}

		auto result = framebuffers_->drawFrame(
			window, *device_, *renderPass_, *context_, *swapchain_,
			[&](VkCommandBuffer /*cb*/, uint32_t imageIndex)
			{
				gpuProfiler_.beginLabel(*context_, "TrianglePass");
				gpuProfiler_.beginFrame(*context_);
				context_->beginRender(*renderPass_, framebuffers_->handles()[imageIndex], swapchain_->extent(),
				                      0.05f, 0.06f, 0.09f, 1.0f);
				context_->bindPipeline(*pipeline_);
				// bind mesh buffers
				if (mesh_) mesh_->bind(*context_);
				// bind descriptor set (UBO)
				if (dset_)
				{
					VkDescriptorSet set = dset_->handle();
					context_->bindDescriptorSets(pipeline_->layout(), 0, &set, 1);
				}
				// draw indexed
				context_->drawIndexed(mesh_ ? mesh_->indexCount() : 0);
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
		swapchain_->recreate(*device_, window, config_.swapchain);
		createRenderPass();
		createFramebuffers();
		createDescriptors();
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

		// Destroy descriptors (pool and layout)
		if (dsp_)
		{
			dsp_->cleanup(*device_);
			dsp_.reset();
		}
		if (dsl_)
		{
			dsl_->cleanup(*device_);
			dsl_.reset();
		}

		if (swapchain_)
		{
			swapchain_->cleanup(*device_);
			swapchain_.reset();
		}

		if (uniformBuffer_)
		{
			uniformBuffer_->cleanup(*device_);
			uniformBuffer_.reset();
		}
		if (vertexBuffer_)
		{
			vertexBuffer_->cleanup(*device_);
			vertexBuffer_.reset();
		}
		if (indexBuffer_)
		{
			indexBuffer_->cleanup(*device_);
			indexBuffer_.reset();
		}
		if (mesh_)
		{
			mesh_->cleanup(*device_);
			mesh_.reset();
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
		swapchain_->create(*device_, window, config_.swapchain);
	}

	void Renderer::createRenderPass()
	{
		renderPass_ = std::make_unique<gfx::RenderPass>();
		// Choose a supported depth format
		VkFormat depthFormat = device_->findDepthFormat();
		renderPass_->create(*device_, swapchain_->imageFormat(), depthFormat);
	}

	void Renderer::createPipeline()
	{
		pipeline_ = std::make_unique<gfx::Pipeline>();
		gfx::PipelineCreateInfo info{};
		info.vsSpvPath = "shaders/triangle.vert.spv";
		info.fsSpvPath = "shaders/triangle.frag.spv";
		info.viewportExtent = swapchain_->extent();
		info.enableDepthTest = config_.pipeline.enableDepthTest ? VK_TRUE : VK_FALSE;
		info.enableDepthWrite = config_.pipeline.enableDepthWrite ? VK_TRUE : VK_FALSE;
		pipeline_->create(*device_, *renderPass_, info);
	}

	void Renderer::createFramebuffers()
	{
		if (!framebuffers_) framebuffers_ = std::make_unique<gfx::Framebuffers>();
		if (!depthImage_) depthImage_ = std::make_unique<gfx::Image>();
		gfx::ImageCreateInfo di{};
		di.width = swapchain_->extent().width;
		di.height = swapchain_->extent().height;
		di.format = device_->findDepthFormat();
		di.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		di.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		di.tiling = VK_IMAGE_TILING_OPTIMAL;
		di.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		depthImage_->cleanup(*device_);
		depthImage_->create(*device_, di);

		framebuffers_->create(*device_, *renderPass_, swapchain_->extent(), swapchain_->imageViews(),
		                      depthImage_->view());
	}

	void Renderer::createGeometry()
	{
		if (!vertexBuffer_) vertexBuffer_ = std::make_unique<gfx::Buffer>();
		if (!mesh_) mesh_ = std::make_unique<gfx::Mesh>();
		mesh_->cleanup(*device_);
		mesh_->createCube(*device_);

		if (!uniformBuffer_) uniformBuffer_ = std::make_unique<gfx::Buffer>();
		gfx::BufferCreateInfo ubi{};
		ubi.size = sizeof(glm::mat4); // MVP
		ubi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		ubi.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		uniformBuffer_->cleanup(*device_);
		uniformBuffer_->create(*device_, ubi);
	}

	void Renderer::createDescriptors()
	{
		// Ensure pipeline object exists
		if (!pipeline_) pipeline_ = std::make_unique<gfx::Pipeline>();

		// Destroy old pipeline (and its layout) BEFORE destroying old set layout
		pipeline_->cleanup(*device_);

		// If a descriptor pool exists, destroy it first (frees sets implicitly)
		if (dsp_) { dsp_->cleanup(*device_); }

		// Destroy old descriptor set layout, then recreate it
		VkDescriptorSetLayoutBinding ubo{};
		ubo.binding = 0;
		ubo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubo.descriptorCount = 1;
		ubo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		if (!dsl_) dsl_ = std::make_unique<gfx::DescriptorSetLayout>();
		dsl_->cleanup(*device_);
		dsl_->create(*device_, &ubo, 1);
		gfx::PipelineCreateInfo info{};
		info.vsSpvPath = "shaders/triangle.vert.spv";
		info.fsSpvPath = "shaders/triangle.frag.spv";
		info.viewportExtent = swapchain_->extent();
		info.enableDepthTest = config_.pipeline.enableDepthTest ? VK_TRUE : VK_FALSE;
		info.enableDepthWrite = config_.pipeline.enableDepthWrite ? VK_TRUE : VK_FALSE;
		VkDescriptorSetLayout layout = dsl_->handle();
		info.setLayouts = &layout;
		info.setLayoutCount = 1;
		// vertex input via VertexLayout (recreate to avoid duplicated attributes)
		vertexLayout_ = std::make_unique<gfx::VertexLayout>();
		// use mesh's layout
		vertexLayout_ = std::make_unique<gfx::VertexLayout>(*mesh_->vertexLayout());
		info.vertexLayout = mesh_->vertexLayout();
		pipeline_->create(*device_, *renderPass_, info);

		// Descriptor pool & set
		if (!dsp_) dsp_ = std::make_unique<gfx::DescriptorPool>();
		dsp_->cleanup(*device_);
		VkDescriptorPoolSize poolSize{};
		poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSize.descriptorCount = 1;
		dsp_->create(*device_, &poolSize, 1, 1);

		if (!dset_) dset_ = std::make_unique<gfx::DescriptorSet>();
		dset_->allocate(*device_, *dsp_, *dsl_);
		dset_->updateUniformBuffer(*device_, 0, uniformBuffer_->handle(), uniformBuffer_->size());
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
		if (depthImage_)
		{
			depthImage_->cleanup(*device_);
			depthImage_.reset();
		}
	}
} // namespace luster
