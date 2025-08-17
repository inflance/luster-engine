#include "core/application.hpp"
#include "core/config.hpp"
#include "core/platform.hpp"
#include <stdexcept>

namespace luster
{
	Application::Application() = default;
	Application::~Application() { cleanup(); }

	void Application::init()
	{
		Log::init();
		spdlog::info("Luster sandbox starting (SDL + Vulkan triangle)...");

		Platform::init();

		constexpr int width = 1280;
		constexpr int height = 720;
		window_ = std::make_unique<Window>(
			"Luster (Vulkan)", width, height, WindowFlags::Vulkan | WindowFlags::Resizable);
		renderer_ = std::make_unique<Renderer>();
		EngineConfig cfg{};
		// 示例：按需修改 present mode 或 FPS 上报周期
		// cfg.swapchain.preferredPresentMode = VK_PRESENT_MODE_MAILBOX_KHR; // 如需低延迟（若可用）
		// cfg.fpsReportIntervalMs = 500.0;
		renderer_->init(*window_, cfg);
	}

	void Application::mainLoop()
	{
		bool running = true;
		bool framebufferResized = false;
		auto last = std::chrono::steady_clock::now();
		// FPS title update
		double accumSeconds = 0.0;
		uint32_t framesSinceUpdate = 0;
		// Pause toggle state (debounced on key press)
		bool paused = false;
		bool prevPToggled = false;
		bool mouseCaptured = false;
		bool prevF1 = false;
		while (running)
		{
			auto now = std::chrono::steady_clock::now();
			float dt = std::chrono::duration<float>(now - last).count();
			last = now;
			running = window_->pollEvents(framebufferResized);
			// Capture input snapshot once per frame
			auto input = Input::captureSnapshot();
			// ESC to quit (independent of event handling)
			{
				// Pause toggle on P press edge
				if (input.pressedP)
				{
					paused = !paused;
				}

				// F1 toggle mouse capture/visibility (edge)
				if (input.pressedF1)
				{
					mouseCaptured = !mouseCaptured;
					Platform::setCursorVisible(!mouseCaptured);
				}

				if (input.keyEsc)
				{
					running = false;
				}
			}
			if (framebufferResized)
			{
				renderer_->recreateSwapchain(*window_);
				framebufferResized = false;
			}

			if (!paused)
			{
				renderer_->update(dt, input);
				if (!renderer_->drawFrame(*window_))
				{
					// On hard failure, exit the loop
					running = false;
				}
			}

			// Update window title with FPS every second
			accumSeconds += static_cast<double>(dt);
			++framesSinceUpdate;
			if (accumSeconds >= 1.0)
			{
				const double fps = static_cast<double>(framesSinceUpdate) / (accumSeconds > 0.0 ? accumSeconds : 1.0);
				char title[160] = {};
				if (!paused)
					std::snprintf(title, sizeof(title), "Luster (Vulkan) - %.1f FPS%s", fps, mouseCaptured ? " [MouseCaptured]" : "");
				else
					std::snprintf(title, sizeof(title), "Luster (Vulkan) - Paused%s", mouseCaptured ? " [MouseCaptured]" : "");
				window_->setTitle(title);
				accumSeconds = 0.0;
				framesSinceUpdate = 0;
			}

			Platform::sleepMs(1);
		}
	}

	void Application::run()
	{
		init();
		mainLoop();
		cleanup();
	}

	void Application::cleanup() const
	{
		if (renderer_)
		{
			renderer_->cleanup();
		}
		Platform::shutdown();
		spdlog::info("Luster sandbox exiting.");
	}
} // namespace luster
