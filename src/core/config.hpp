#pragma once

#include "core/gfx/device.hpp"
#include "core/gfx/swapchain.hpp"

namespace luster
{
	struct EngineConfig
	{
		gfx::Device::InitParams device{};
		gfx::SwapchainCreateInfo swapchain{}; // preferredPresentMode 可设置为 FIFO/MAILBOX 等
		double fpsReportIntervalMs = 500.0; // FPS 输出间隔
		struct PipelineOptions
		{
			bool enableDepthTest = true;
			bool enableDepthWrite = true;
		} pipeline;
	};
}
