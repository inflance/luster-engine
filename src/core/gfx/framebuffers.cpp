#include "core/gfx/framebuffers.hpp"
#include "core/gfx/device.hpp"
#include "core/gfx/render_pass.hpp"
#include <stdexcept>

namespace luster::gfx
{
    void Framebuffers::create(const Device& device, const RenderPass& rp, VkExtent2D extent, const std::vector<VkImageView>& imageViews)
    {
        cleanup(device);
        framebuffers_.resize(imageViews.size());
        for (size_t i = 0; i < imageViews.size(); ++i)
        {
            VkImageView attachments[] = {imageViews[i]};
            VkFramebufferCreateInfo fci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            fci.renderPass = rp.handle();
            fci.attachmentCount = 1;
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
}


