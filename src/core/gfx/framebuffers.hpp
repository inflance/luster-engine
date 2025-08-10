#pragma once

#include "core/core.hpp"
#include <vector>
#include <functional>

namespace luster::gfx
{
    class Device;
    class RenderPass;

    class Framebuffers
    {
    public:
        Framebuffers() = default;
        ~Framebuffers() = default;

        void create(const Device& device,
                    const RenderPass& rp,
                    VkExtent2D extent,
                    const std::vector<VkImageView>& colorImageViews,
                    VkImageView depthImageView);
        enum class FrameResult { Ok, NeedRecreate, Error };
        // acquire image + record + submit + present 的便捷接口：传入回调以录制绘制命令
        // recordCallback(VkCommandBuffer, uint32_t imageIndex)
        FrameResult drawFrame(SDL_Window* window,
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


