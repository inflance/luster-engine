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
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

	bool Renderer::drawFrame(SDL_Window* window)
	{
        // Update MVP (rotation over time)
        static auto t0 = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        float seconds = std::chrono::duration<float>(now - t0).count();
        glm::mat4 proj = glm::ortho(-1.2f, 1.2f, -0.9f, 0.9f);
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), seconds, glm::vec3(0, 0, 1));
        glm::mat4 mvp = proj * view * model;
        if (uniformBuffer_)
        {
            void* p = uniformBuffer_->map(*device_);
            memcpy(p, &mvp, sizeof(mvp));
            uniformBuffer_->unmap(*device_);
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
                // bind vertex buffer
                const VkBuffer vbs[] = { vertexBuffer_ ? vertexBuffer_->handle() : VK_NULL_HANDLE };
                const VkDeviceSize ofs[] = { 0 };
                if (vbs[0] != VK_NULL_HANDLE) context_->bindVertexBuffers(0, vbs, ofs, 1);
                // bind descriptor set (UBO)
                if (descriptorSet_) context_->bindDescriptorSets(pipeline_->layout(), 0, &descriptorSet_, 1);
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
        if (descriptorPool_) { vkDestroyDescriptorPool(device_->logical(), descriptorPool_, nullptr); descriptorPool_ = VK_NULL_HANDLE; }
        if (descriptorSetLayout_) { vkDestroyDescriptorSetLayout(device_->logical(), descriptorSetLayout_, nullptr); descriptorSetLayout_ = VK_NULL_HANDLE; }

		if (swapchain_)
		{
			swapchain_->cleanup(*device_);
			swapchain_.reset();
		}

        if (uniformBuffer_) { uniformBuffer_->cleanup(*device_); uniformBuffer_.reset(); }
        if (vertexBuffer_) { vertexBuffer_->cleanup(*device_); vertexBuffer_.reset(); }
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
		// Choose a common depth format (fallback chain can be added later)
		VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
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
		di.format = VK_FORMAT_D32_SFLOAT;
		di.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		di.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		di.tiling = VK_IMAGE_TILING_OPTIMAL;
		di.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthImage_->cleanup(*device_);
        depthImage_->create(*device_, di);

        framebuffers_->create(*device_, *renderPass_, swapchain_->extent(), swapchain_->imageViews(), depthImage_->view());
	}

    void Renderer::createGeometry()
    {
        if (!vertexBuffer_) vertexBuffer_ = std::make_unique<gfx::Buffer>();
        // 2D pos + RGB color
        struct V { float px, py; float r, g, b; };
        const V verts[] = {
            { 0.0f, -0.6f, 1.0f, 0.2f, 0.2f },
            { 0.6f,  0.6f, 0.2f, 1.0f, 0.2f },
            { -0.6f, 0.6f, 0.2f, 0.2f, 1.0f }
        };
        gfx::BufferCreateInfo bi{};
        bi.size = sizeof(verts);
        bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bi.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        vertexBuffer_->cleanup(*device_);
        vertexBuffer_->create(*device_, bi);
        void* data = vertexBuffer_->map(*device_);
        memcpy(data, verts, sizeof(verts));
        vertexBuffer_->unmap(*device_);

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
        if (descriptorPool_) { vkDestroyDescriptorPool(device_->logical(), descriptorPool_, nullptr); descriptorPool_ = VK_NULL_HANDLE; }

        // Destroy old descriptor set layout, then recreate it
        if (descriptorSetLayout_) { vkDestroyDescriptorSetLayout(device_->logical(), descriptorSetLayout_, nullptr); descriptorSetLayout_ = VK_NULL_HANDLE; }
        VkDescriptorSetLayoutBinding ubo{};
        ubo.binding = 0;
        ubo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubo.descriptorCount = 1;
        ubo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        lci.bindingCount = 1; lci.pBindings = &ubo;
        VkResult r = vkCreateDescriptorSetLayout(device_->logical(), &lci, nullptr, &descriptorSetLayout_);
        if (r != VK_SUCCESS) throw std::runtime_error("vkCreateDescriptorSetLayout failed");
        gfx::PipelineCreateInfo info{};
        info.vsSpvPath = "shaders/triangle.vert.spv";
        info.fsSpvPath = "shaders/triangle.frag.spv";
        info.viewportExtent = swapchain_->extent();
        info.enableDepthTest = config_.pipeline.enableDepthTest ? VK_TRUE : VK_FALSE;
        info.enableDepthWrite = config_.pipeline.enableDepthWrite ? VK_TRUE : VK_FALSE;
        info.setLayouts = &descriptorSetLayout_;
        info.setLayoutCount = 1;
        // vertex input
        static VkVertexInputBindingDescription bind{0, sizeof(float)*5, VK_VERTEX_INPUT_RATE_VERTEX};
        static VkVertexInputAttributeDescription attrs[] = {
            {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},                 // aPosition
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float)*2} // aColor
        };
        info.vertexBinding = &bind; info.vertexBindingCount = 1;
        info.vertexAttributes = attrs; info.vertexAttributeCount = 2;
        pipeline_->create(*device_, *renderPass_, info);

        // Descriptor pool & set
        VkDescriptorPoolSize poolSize{}; poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; poolSize.descriptorCount = 1;
        VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pci.maxSets = 1; pci.poolSizeCount = 1; pci.pPoolSizes = &poolSize;
        r = vkCreateDescriptorPool(device_->logical(), &pci, nullptr, &descriptorPool_);
        if (r != VK_SUCCESS) throw std::runtime_error("vkCreateDescriptorPool failed");

        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = descriptorPool_; ai.descriptorSetCount = 1; ai.pSetLayouts = &descriptorSetLayout_;
        r = vkAllocateDescriptorSets(device_->logical(), &ai, &descriptorSet_);
        if (r != VK_SUCCESS) throw std::runtime_error("vkAllocateDescriptorSets failed");

        VkDescriptorBufferInfo dbi{}; dbi.buffer = uniformBuffer_->handle(); dbi.offset = 0; dbi.range = uniformBuffer_->size();
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = descriptorSet_;
        write.dstBinding = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &dbi;
        vkUpdateDescriptorSets(device_->logical(), 1, &write, 0, nullptr);
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
