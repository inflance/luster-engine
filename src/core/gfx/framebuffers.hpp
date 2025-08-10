#pragma once

#include "core/core.hpp"
#include <vector>

namespace luster::gfx
{
    class Device;
    class RenderPass;

    class Framebuffers
    {
    public:
        Framebuffers() = default;
        ~Framebuffers() = default;

        void create(const Device& device, const RenderPass& rp, VkExtent2D extent, const std::vector<VkImageView>& imageViews);
        void cleanup(const Device& device);

        const std::vector<VkFramebuffer>& handles() const { return framebuffers_; }

    private:
        std::vector<VkFramebuffer> framebuffers_{};
    };
}


