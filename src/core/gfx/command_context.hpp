#pragma once

#include "core/core.hpp"

namespace luster::gfx
{
    class Device; class RenderPass; class Pipeline;
    class GpuProfiler;

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
        void beginRender(const RenderPass& rp, VkFramebuffer framebuffer, VkExtent2D extent,
                         const VkClearValue* clears, uint32_t clearCount);
        void endRender();
        // Debug label helpers (no-op if extension not present)
        void beginLabel(const char* name, float r = 0.2f, float g = 0.6f, float b = 0.9f, float a = 1.0f);
        void endLabel();
        void bindPipeline(const Pipeline& pipeline);
        void bindVertexBuffers(uint32_t firstBinding, const VkBuffer* buffers, const VkDeviceSize* offsets, uint32_t count);
        void bindDescriptorSets(VkPipelineLayout layout, uint32_t firstSet, const VkDescriptorSet* sets, uint32_t count);
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
        friend class GpuProfiler;
        VkCommandPool cmdPool_ = VK_NULL_HANDLE;
        VkCommandBuffer cmdBuf_ = VK_NULL_HANDLE;
        VkSemaphore semImageAvailable_ = VK_NULL_HANDLE;
        VkSemaphore semRenderFinished_ = VK_NULL_HANDLE;
        VkFence inFlight_ = VK_NULL_HANDLE;
        VkDevice device_ = VK_NULL_HANDLE;

        // cached for beginRender/endRender
        bool renderPassOpen_ = false;
    };
}


