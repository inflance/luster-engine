// Copyright (c) Luster
#pragma once

#include "core/core.hpp"
#include <string>
#include <cstdint>

namespace luster
{
	enum class WindowFlags : uint32_t
	{
		None      = 0,
		Vulkan    = 1u << 0,
		Resizable = 1u << 1,
		HighDPI   = 1u << 2,
		Hidden    = 1u << 3,
		Fullscreen= 1u << 4,
		Borderless= 1u << 5,
	};

	inline constexpr WindowFlags operator|(WindowFlags a, WindowFlags b)
	{
		return static_cast<WindowFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
	}

	inline constexpr WindowFlags operator&(WindowFlags a, WindowFlags b)
	{
		return static_cast<WindowFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
	}

	inline WindowFlags& operator|=(WindowFlags& a, WindowFlags b)
	{
		a = a | b;
		return a;
	}

	class Window
	{
	public:
		Window(const char* title, int width, int height, WindowFlags flags);
		~Window();

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		SDL_Window* sdl() const { return window_; }

		void cleanup();
		// Convenience helpers
		void getSize(int& width, int& height) const;
		bool pollEvents(bool& framebufferResized);
		void setTitle(const char* title);

		// Utility: create Vulkan surface from this window
		VkSurfaceKHR createVulkanSurface(VkInstance instance) const;

	private:
		SDL_Window* window_ = nullptr;
	};
} // namespace luster
