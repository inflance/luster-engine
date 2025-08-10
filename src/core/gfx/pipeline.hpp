#pragma once

#include "core/core.hpp"
#include <string>

namespace luster::gfx
{
    class Device;
    class RenderPass;

    struct PipelineCreateInfo
    {
        std::string vsSpvPath;
        std::string fsSpvPath;
        VkExtent2D viewportExtent{};
    };

    class Pipeline
    {
    public:
        Pipeline() = default;
        ~Pipeline() = default;

        void create(const Device& device, const RenderPass& rp, const PipelineCreateInfo& info);
        void cleanup(const Device& device);

        VkPipelineLayout layout() const { return pipelineLayout_; }
        VkPipeline handle() const { return pipeline_; }

    private:
        VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
    };
}


