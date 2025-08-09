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

	Window::Window(const char* title, int width, int height, Uint32 flags)
	{
		window_ = SDL_CreateWindow(title, width, height, flags);
		if (!window_)
		{
			log_sdl_error_local("SDL_CreateWindow failed");
			throw std::runtime_error("SDL_CreateWindow failed");
		}
	}

	Window::~Window()
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
} // namespace luster
