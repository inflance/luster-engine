#include "core/gfx/framebuffers.hpp"
#include "core/gfx/device.hpp"
#include "core/gfx/render_pass.hpp"
#include "core/gfx/command_context.hpp"
#include "core/gfx/swapchain.hpp"
#include "core/utils/profiler.hpp"
#include <stdexcept>
#include <functional>

namespace luster::gfx
{
    void Framebuffers::create(const Device& device,
                              const RenderPass& rp,
                              VkExtent2D extent,
                              const std::vector<VkImageView>& colorImageViews,
                              VkImageView depthImageView)
    {
        cleanup(device);
        framebuffers_.resize(colorImageViews.size());
        for (size_t i = 0; i < colorImageViews.size(); ++i)
        {
            VkImageView attachments[] = {colorImageViews[i], depthImageView};
            VkFramebufferCreateInfo fci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            fci.renderPass = rp.handle();
            fci.attachmentCount = 2;
            fci.pAttachments = attachments;
            fci.width = extent.width;
            fci.height = extent.height;
            fci.layers = 1;
            VkResult r = vkCreateFramebuffer(device.logical(), &fci, nullptr, &framebuffers_[i]);
            if (r != VK_SUCCESS) throw std::runtime_error("vkCreateFramebuffer failed");
        }
    }

    void Framebuffers::cleanup(const Device& device)
    {
        for (auto fb : framebuffers_) if (fb) vkDestroyFramebuffer(device.logical(), fb, nullptr);
        framebuffers_.clear();
        
    }

    Framebuffers::FrameResult Framebuffers::drawFrame(SDL_Window* window,
                                                      const Device& device,
                                                      const RenderPass& rp,
                                                      CommandContext& ctx,
                                                      Swapchain& swapchain,
                                                      const std::function<void(VkCommandBuffer, uint32_t)>& recordCallback)
    {
        (void)window; (void)rp; // 当前实现不直接使用，保留签名以便未来扩展
        PROFILE_SCOPE("frame");
        ctx.waitFence(device, UINT64_C(1'000'000'000));
        ctx.resetFence(device);

        uint32_t imageIndex = 0;
        VkResult r = vkAcquireNextImageKHR(device.logical(), swapchain.handle(), UINT64_MAX,
                                           ctx.imageAvailable(), VK_NULL_HANDLE, &imageIndex);
        if (r == VK_ERROR_OUT_OF_DATE_KHR)
            return FrameResult::NeedRecreate; // signal caller to recreate swapchain
        if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR)
            return FrameResult::Error;

        VkCommandBuffer cb = ctx.begin();
        recordCallback(cb, imageIndex);
        ctx.end();

        ctx.submit(device.gfxQueue(), ctx.imageAvailable(), ctx.renderFinished(), ctx.inFlight());

        VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        pi.waitSemaphoreCount = 1;
        VkSemaphore rf = ctx.renderFinished();
        pi.pWaitSemaphores = &rf;
        pi.swapchainCount = 1;
        VkSwapchainKHR sc = swapchain.handle();
        pi.pSwapchains = &sc;
        pi.pImageIndices = &imageIndex;
        r = vkQueuePresentKHR(device.presentQueue(), &pi);
        if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR)
            return FrameResult::NeedRecreate;
        if (r != VK_SUCCESS)
            return FrameResult::Error;

        return FrameResult::Ok;
    }
}


