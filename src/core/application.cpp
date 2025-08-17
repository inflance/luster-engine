#include "core/application.hpp"
#include "core/config.hpp"
#include <stdexcept>

namespace luster
{
	// SDL error logging helper
	static void log_sdl_error(const char* what)
	{
		const char* err = SDL_GetError();
		spdlog::error("{}: {}", what, (err && *err) ? err : "unknown");
	}

	Application::Application() = default;
	Application::~Application() { cleanup(); }

	void Application::init()
	{
		Log::init();
		spdlog::info("Luster sandbox starting (SDL + Vulkan triangle)...");

#if defined(_WIN32)
		SDL_RegisterApp("Luster", 0, GetModuleHandleW(nullptr));
#endif

		if (!SDL_Init(SDL_INIT_VIDEO))
		{
			log_sdl_error("SDL_Init(SDL_INIT_VIDEO) failed");
			throw std::runtime_error("SDL_Init failed");
		}

		constexpr int width = 1280;
		constexpr int height = 720;
		window_ = std::make_unique<Window>(
			"Luster (Vulkan)", width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
		renderer_ = std::make_unique<Renderer>();
		EngineConfig cfg{};
		// 示例：按需修改 present mode 或 FPS 上报周期
		// cfg.swapchain.preferredPresentMode = VK_PRESENT_MODE_MAILBOX_KHR; // 如需低延迟（若可用）
		// cfg.fpsReportIntervalMs = 500.0;
		renderer_->init(window_->sdl(), cfg);
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
		while (running)
		{
			auto now = std::chrono::steady_clock::now();
			float dt = std::chrono::duration<float>(now - last).count();
			last = now;
			running = window_->pollEvents(framebufferResized);
			// ESC to quit (independent of event handling)
			{
				const bool* keyState = SDL_GetKeyboardState(nullptr);
				// Pause toggle on P press (debounced)
				bool pDown = keyState && keyState[SDL_SCANCODE_P];
				if (pDown && !prevPToggled)
				{
					paused = !paused;
				}
				prevPToggled = pDown;

				if (keyState && keyState[SDL_SCANCODE_ESCAPE])
				{
					running = false;
				}
			}
			if (framebufferResized)
			{
				renderer_->recreateSwapchain(window_->sdl());
				framebufferResized = false;
			}

			if (!paused)
			{
				renderer_->update(dt);
				if (!renderer_->drawFrame(window_->sdl()))
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
					std::snprintf(title, sizeof(title), "Luster (Vulkan) - %.1f FPS", fps);
				else
					std::snprintf(title, sizeof(title), "Luster (Vulkan) - Paused");
				SDL_SetWindowTitle(window_->sdl(), title);
				accumSeconds = 0.0;
				framesSinceUpdate = 0;
			}

			SDL_Delay(1);
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
		SDL_Quit();
		spdlog::info("Luster sandbox exiting.");
	}
} // namespace luster
