#include "core/gfx/gpu_profiler.hpp"
#include "core/gfx/device.hpp"
#include "core/gfx/command_context.hpp"

namespace luster::gfx
{
    void GpuProfiler::init(const Device& device)
    {
        if (queryPool_) return;
        VkQueryPoolCreateInfo qpci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        qpci.queryType = VK_QUERY_TYPE_TIMESTAMP;
        qpci.queryCount = 2;
        vkCreateQueryPool(device.logical(), &qpci, nullptr, &queryPool_);
    }

    void GpuProfiler::cleanup(const Device& device)
    {
        if (queryPool_)
        {
            vkDestroyQueryPool(device.logical(), queryPool_, nullptr);
            queryPool_ = VK_NULL_HANDLE;
        }
    }

    void GpuProfiler::beginFrame(CommandContext& ctx)
    {
        if (!queryPool_) return;
        vkCmdResetQueryPool(ctx.cmdBuf_, queryPool_, 0, 2);
        vkCmdWriteTimestamp(ctx.cmdBuf_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool_, 0);
    }

    void GpuProfiler::endFrame(CommandContext& ctx)
    {
        if (!queryPool_) return;
        vkCmdWriteTimestamp(ctx.cmdBuf_, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool_, 1);
    }

    bool GpuProfiler::getLastTimingMs(const Device& device, double& outMs) const
    {
        if (!queryPool_) return false;
        uint64_t data[2] = {0, 0};
        VkResult qr = vkGetQueryPoolResults(device.logical(), queryPool_, 0, 2, sizeof(data), data, sizeof(uint64_t),
                                            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
        if (qr != VK_SUCCESS || data[1] <= data[0]) return false;
        const double ticks = static_cast<double>(data[1] - data[0]);
        const double ns = ticks * static_cast<double>(device.timestampPeriod());
        outMs = ns / 1.0e6;
        return true;
    }

    void GpuProfiler::beginLabel(CommandContext& ctx, const char* name, const ColorRGBA& color)
    {
        auto pfn = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetDeviceProcAddr(ctx.device_, "vkCmdBeginDebugUtilsLabelEXT"));
        if (!pfn) return;
        VkDebugUtilsLabelEXT label{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
        label.pLabelName = name;
        label.color[0] = color.r; label.color[1] = color.g; label.color[2] = color.b; label.color[3] = color.a;
        pfn(ctx.cmdBuf_, &label);
    }

    void GpuProfiler::endLabel(CommandContext& ctx)
    {
        auto pfn = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetDeviceProcAddr(ctx.device_, "vkCmdEndDebugUtilsLabelEXT"));
        if (!pfn) return;
        pfn(ctx.cmdBuf_);
    }
}


