// Copyright (c) Luster
#include "core/window.hpp"
#include <stdexcept>

namespace luster
{
	static void log_sdl_error_local(const char* what)
	{
		const char* err = SDL_GetError();
		spdlog::error("{}: {}", what, (err && *err) ? err : "unknown");
	}

	static Uint32 to_sdl_flags(WindowFlags flags)
	{
		Uint32 s = 0;
		if ((flags & WindowFlags::Vulkan) == WindowFlags::Vulkan) s |= SDL_WINDOW_VULKAN;
		if ((flags & WindowFlags::Resizable) == WindowFlags::Resizable) s |= SDL_WINDOW_RESIZABLE;
		if ((flags & WindowFlags::HighDPI) == WindowFlags::HighDPI) s |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
		if ((flags & WindowFlags::Hidden) == WindowFlags::Hidden) s |= SDL_WINDOW_HIDDEN;
		if ((flags & WindowFlags::Fullscreen) == WindowFlags::Fullscreen) s |= SDL_WINDOW_FULLSCREEN;
		if ((flags & WindowFlags::Borderless) == WindowFlags::Borderless) s |= SDL_WINDOW_BORDERLESS;
		return s;
	}

	Window::Window(const char* title, int width, int height, WindowFlags flags)
	{
		window_ = SDL_CreateWindow(title, width, height, to_sdl_flags(flags));
		if (!window_)
		{
			log_sdl_error_local("SDL_CreateWindow failed");
			throw std::runtime_error("SDL_CreateWindow failed");
		}
	}

	Window::~Window()
	{
		cleanup();
	}

	void Window::cleanup()
	{
		if (window_)
		{
			SDL_DestroyWindow(window_);
			window_ = nullptr;
		}
	}

	void Window::getSize(int& width, int& height) const
	{
		width = 0;
		height = 0;
		if (window_)
		{
			SDL_GetWindowSize(window_, &width, &height);
		}
	}

	bool Window::pollEvents(bool& framebufferResized)
	{
		bool running = true;
		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			switch (e.type)
			{
			case SDL_EVENT_QUIT:
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				running = false;
				break;
			case SDL_EVENT_WINDOW_RESIZED:
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
				framebufferResized = true;
				break;
			default:
				break;
			}
		}
		return running;
	}

	VkSurfaceKHR Window::createVulkanSurface(VkInstance instance) const
	{
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		if (!SDL_Vulkan_CreateSurface(window_, instance, nullptr, &surface) || surface == VK_NULL_HANDLE)
		{
			throw std::runtime_error("SDL_Vulkan_CreateSurface failed");
		}
		return surface;
	}

	void Window::setTitle(const char* title)
	{
		if (window_)
		{
			SDL_SetWindowTitle(window_, title);
		}
	}
} // namespace luster
