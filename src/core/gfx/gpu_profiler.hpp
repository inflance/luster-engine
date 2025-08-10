#pragma once

#include "core/core.hpp"
#include "core/types.hpp"

namespace luster::gfx
{
    class Device;
    class CommandContext;

    class GpuProfiler
    {
    public:
        GpuProfiler() = default;
        ~GpuProfiler() = default;

        void init(const Device& device);
        void cleanup(const Device& device);

        void beginFrame(CommandContext& ctx);
        void endFrame(CommandContext& ctx);

        // Returns true if timing is available and writes milliseconds into outMs
        bool getLastTimingMs(const Device& device, double& outMs) const;

        // Debug label helpers (no-op if extension not present)
        void beginLabel(CommandContext& ctx, const char* name, const ColorRGBA& color = {0.2f,0.6f,0.9f,1.0f});
        void endLabel(CommandContext& ctx);

    private:
        VkQueryPool queryPool_ = VK_NULL_HANDLE;
        mutable uint64_t lastBegin_ = 0;
        mutable uint64_t lastEnd_ = 0;
    };
}


