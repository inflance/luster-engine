// Copyright (c) Luster
#include "core/platform.hpp"
#include "core/core.hpp"
#include <stdexcept>

namespace luster
{
	namespace Platform
	{
		static void log_sdl_error_local(const char* what)
		{
			const char* err = SDL_GetError();
			spdlog::error("{}: {}", what, (err && *err) ? err : "unknown");
		}

		void init()
		{
#if defined(_WIN32)
			SDL_RegisterApp("Luster", 0, GetModuleHandleW(nullptr));
#endif
			if (!SDL_Init(SDL_INIT_VIDEO))
			{
				log_sdl_error_local("SDL_Init(SDL_INIT_VIDEO) failed");
				throw std::runtime_error("SDL_Init failed");
			}
		}

		void shutdown()
		{
			SDL_Quit();
		}

		void setCursorVisible(bool visible)
		{
			if (visible)
				SDL_ShowCursor();
			else
				SDL_HideCursor();
		}

		void sleepMs(uint32_t milliseconds)
		{
			SDL_Delay(static_cast<Uint32>(milliseconds));
		}
	}
}


