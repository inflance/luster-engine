// Copyright (c) Luster
#pragma once

#include "core/core.hpp"
#include <string>

namespace luster
{
	class Window
	{
	public:
		Window(const char* title, int width, int height, Uint32 flags);
		~Window();

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		SDL_Window* sdl() const { return window_; }

		void cleanup();
		// Convenience helpers
		void getSize(int& width, int& height) const;
		bool pollEvents(bool& framebufferResized);

		// Utility: create Vulkan surface from this window
		VkSurfaceKHR createVulkanSurface(VkInstance instance) const;

	private:
		SDL_Window* window_ = nullptr;
	};
} // namespace luster
