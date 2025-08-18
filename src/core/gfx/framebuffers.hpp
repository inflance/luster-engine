#pragma once

#include "core/core.hpp"
#include <vector>
#include <functional>

namespace luster { class Window; }

namespace luster::gfx
{
	class Device;
	class RenderPass;

	class Framebuffers
	{
	public:
		enum class FrameResult { Ok, NeedRecreate, Error };

		Framebuffers() = default;
		~Framebuffers() = default;

		void create(const Device& device,
		            const RenderPass& rp,
		            VkExtent2D extent,
		            const std::vector<VkImageView>& colorImageViews,
		            VkImageView depthImageView);


		FrameResult drawFrame(::luster::Window& window,
		                      const Device& device,
		                      const RenderPass& rp,
		                      class CommandContext& ctx,
		                      class Swapchain& swapchain,
		                      const std::function<void(VkCommandBuffer, uint32_t)>& recordCallback);

		void cleanup(const Device& device);

		const std::vector<VkFramebuffer>& handles() const { return framebuffers_; }

	private:
		std::vector<VkFramebuffer> framebuffers_{};
		VkImageView depthView_ = VK_NULL_HANDLE; // cached only for signature parity; not owned
	};
}
