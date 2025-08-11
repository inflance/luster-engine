#include "core/gfx/command_context.hpp"
#include "core/gfx/device.hpp"
#include "core/gfx/render_pass.hpp"
#include "core/gfx/pipeline.hpp"
#include <stdexcept>
#include "core/utils/profiler.hpp"

namespace luster::gfx
{
	static const char* vk_err(VkResult r)
	{
		switch (r)
		{
		case VK_SUCCESS: return "VK_SUCCESS";
		default: return "VK_ERROR";
		}
	}

	void CommandContext::create(const Device& device, uint32_t queueFamilyIndex)
	{
		device_ = device.logical();
		VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
		pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		pci.queueFamilyIndex = queueFamilyIndex;
		VkResult r = vkCreateCommandPool(device.logical(), &pci, nullptr, &cmdPool_);
		if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkCreateCommandPool failed: ") + vk_err(r));

		VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
		ai.commandPool = cmdPool_;
		ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		ai.commandBufferCount = 1;
		r = vkAllocateCommandBuffers(device.logical(), &ai, &cmdBuf_);
		if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkAllocateCommandBuffers failed: ") + vk_err(r));
	}

	void CommandContext::createSync(const Device& device)
	{
		VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
		VkResult r = vkCreateSemaphore(device.logical(), &sci, nullptr, &semImageAvailable_);
		if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkCreateSemaphore failed: ") + vk_err(r));
		r = vkCreateSemaphore(device.logical(), &sci, nullptr, &semRenderFinished_);
		if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkCreateSemaphore failed: ") + vk_err(r));

		VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
		fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		r = vkCreateFence(device.logical(), &fci, nullptr, &inFlight_);
		if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkCreateFence failed: ") + vk_err(r));
	}

	void CommandContext::cleanup(const Device& device)
	{
		if (inFlight_) vkDestroyFence(device.logical(), inFlight_, nullptr);
		if (semRenderFinished_) vkDestroySemaphore(device.logical(), semRenderFinished_, nullptr);
		if (semImageAvailable_) vkDestroySemaphore(device.logical(), semImageAvailable_, nullptr);
		if (cmdBuf_) vkFreeCommandBuffers(device.logical(), cmdPool_, 1, &cmdBuf_);
		if (cmdPool_) vkDestroyCommandPool(device.logical(), cmdPool_, nullptr);
		device_ = VK_NULL_HANDLE;
		cmdPool_ = VK_NULL_HANDLE;
		cmdBuf_ = VK_NULL_HANDLE;
		semImageAvailable_ = VK_NULL_HANDLE;
		semRenderFinished_ = VK_NULL_HANDLE;
		inFlight_ = VK_NULL_HANDLE;
	}

	void CommandContext::waitFence(const Device& device, uint64_t timeoutNs) const
	{
		VkResult r = vkWaitForFences(device.logical(), 1, &inFlight_, VK_TRUE, timeoutNs);
		if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkWaitForFences failed: ") + vk_err(r));
	}

	void CommandContext::resetFence(const Device& device) const
	{
		vkResetFences(device.logical(), 1, &inFlight_);
	}

	VkCommandBuffer CommandContext::begin()
	{
		PROFILE_SCOPE("cmd_begin");
		vkResetCommandBuffer(cmdBuf_, 0);
		VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		VkResult r = vkBeginCommandBuffer(cmdBuf_, &bi);
		if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkBeginCommandBuffer failed: ") + vk_err(r));
		return cmdBuf_;
	}

	void CommandContext::end()
	{
		PROFILE_SCOPE("cmd_end");
		VkResult r = vkEndCommandBuffer(cmdBuf_);
		if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkEndCommandBuffer failed: ") + vk_err(r));
	}

	void CommandContext::beginRender(const RenderPass& rp, VkFramebuffer framebuffer, VkExtent2D extent,
	                                 const VkClearValue& clear)
	{
		VkClearValue clears[2]{};
		clears[0] = clear; // color
		clears[1].depthStencil.depth = 1.0f; // default depth clear
		clears[1].depthStencil.stencil = 0;

		VkRenderPassBeginInfo rpbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
		rpbi.renderPass = rp.handle();
		rpbi.framebuffer = framebuffer;
		rpbi.renderArea.offset = {0, 0};
		rpbi.renderArea.extent = extent;
		rpbi.clearValueCount = 2;
		rpbi.pClearValues = clears;
		vkCmdBeginRenderPass(cmdBuf_, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
		renderPassOpen_ = true;
	}

	void CommandContext::beginRender(const RenderPass& rp, VkFramebuffer framebuffer, VkExtent2D extent,
	                                 float r, float g, float b, float a)
	{
		VkClearValue clear{};
		clear.color = {{r, g, b, a}};
		beginRender(rp, framebuffer, extent, clear);
	}

	void CommandContext::endRender()
	{
		if (renderPassOpen_)
		{
			vkCmdEndRenderPass(cmdBuf_);
			renderPassOpen_ = false;
		}
	}

	void CommandContext::beginLabel(const char* name, float r, float g, float b, float a)
	{
		if (!device_) return;
		auto pfn = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetDeviceProcAddr(
			device_, "vkCmdBeginDebugUtilsLabelEXT"));
		if (!pfn) return;
		VkDebugUtilsLabelEXT label{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
		label.pLabelName = name;
		label.color[0] = r;
		label.color[1] = g;
		label.color[2] = b;
		label.color[3] = a;
		pfn(cmdBuf_, &label);
	}

	void CommandContext::endLabel()
	{
		if (!device_) return;
		auto pfn = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetDeviceProcAddr(
			device_, "vkCmdEndDebugUtilsLabelEXT"));
		if (!pfn) return;
		pfn(cmdBuf_);
	}

	void CommandContext::bindPipeline(const Pipeline& pipeline)
	{
		vkCmdBindPipeline(cmdBuf_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle());
	}

	void CommandContext::bindVertexBuffers(uint32_t firstBinding, const VkBuffer* buffers, const VkDeviceSize* offsets,
	                                       uint32_t count)
	{
		vkCmdBindVertexBuffers(cmdBuf_, firstBinding, count, buffers, offsets);
	}

	void CommandContext::bindDescriptorSets(VkPipelineLayout layout, uint32_t firstSet, const VkDescriptorSet* sets,
	                                        uint32_t count)
	{
		vkCmdBindDescriptorSets(cmdBuf_, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, firstSet, count, sets, 0, nullptr);
	}

	void CommandContext::bindIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType)
	{
		vkCmdBindIndexBuffer(cmdBuf_, buffer, offset, indexType);
	}

	void CommandContext::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
	                          uint32_t firstInstance)
	{
		vkCmdDraw(cmdBuf_, vertexCount, instanceCount, firstVertex, firstInstance);
	}

	void CommandContext::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
	                                 int32_t vertexOffset, uint32_t firstInstance)
	{
		vkCmdDrawIndexed(cmdBuf_, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	}

	void CommandContext::submit(VkQueue gfxQueue, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore,
	                            VkFence fence) const
	{
		VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
		si.waitSemaphoreCount = waitSemaphore ? 1u : 0u;
		si.pWaitSemaphores = waitSemaphore ? &waitSemaphore : nullptr;
		si.pWaitDstStageMask = waitSemaphore ? &waitStages : nullptr;
		si.commandBufferCount = 1;
		si.pCommandBuffers = &cmdBuf_;
		si.signalSemaphoreCount = signalSemaphore ? 1u : 0u;
		si.pSignalSemaphores = signalSemaphore ? &signalSemaphore : nullptr;
		VkResult r = vkQueueSubmit(gfxQueue, 1, &si, fence);
		if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkQueueSubmit failed: ") + vk_err(r));
	}
}
