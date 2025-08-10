#pragma once

#include "core/core.hpp"

namespace luster::gfx
{
    class Device; class RenderPass; class Pipeline;

    class CommandContext
    {
    public:
        CommandContext() = default;
        ~CommandContext() = default;

        void create(const Device& device, uint32_t queueFamilyIndex);
        void createSync(const Device& device);
        void cleanup(const Device& device);

        void waitFence(const Device& device, uint64_t timeoutNs = UINT64_C(1'000'000'000)) const;
        void resetFence(const Device& device) const;

        VkCommandBuffer begin();
        void end();

        // High-level render helpers
        void beginRender(const RenderPass& rp, VkFramebuffer framebuffer, VkExtent2D extent, const VkClearValue& clear);
        void beginRender(const RenderPass& rp, VkFramebuffer framebuffer, VkExtent2D extent,
                         float r, float g, float b, float a);
        void endRender();
        void bindPipeline(const Pipeline& pipeline);
        void draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0);

        // Submit current command buffer with common triangle pipeline usage
        void submit(VkQueue gfxQueue, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence fence) const;

        // Accessors
        VkCommandPool commandPool() const { return cmdPool_; }
        VkCommandBuffer commandBuffer() const { return cmdBuf_; }
        VkSemaphore imageAvailable() const { return semImageAvailable_; }
        VkSemaphore renderFinished() const { return semRenderFinished_; }
        VkFence inFlight() const { return inFlight_; }

    private:
        VkCommandPool cmdPool_ = VK_NULL_HANDLE;
        VkCommandBuffer cmdBuf_ = VK_NULL_HANDLE;
        VkSemaphore semImageAvailable_ = VK_NULL_HANDLE;
        VkSemaphore semRenderFinished_ = VK_NULL_HANDLE;
        VkFence inFlight_ = VK_NULL_HANDLE;

        // cached for beginRender/endRender
        bool renderPassOpen_ = false;
    };
}


