#pragma once

#include "core/core.hpp"

namespace luster::gfx
{
    class Device;

    class RenderPass
    {
    public:
        RenderPass() = default;
        ~RenderPass() = default;

        void create(const Device& device, VkFormat colorFormat, VkFormat depthFormat);
        void cleanup(const Device& device);

        VkRenderPass handle() const { return renderPass_; }

    private:
        VkRenderPass renderPass_ = VK_NULL_HANDLE;
    };
}


