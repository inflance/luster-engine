// Copyright (c) Luster
#pragma once

#include "renderer.hpp"
#include "core/core.hpp"
#include "core/window.hpp"

namespace luster
{
	class Application
	{
	public:
		Application();
		~Application();

		void run();

	private:
		void init();
		void mainLoop();
		void cleanup() const;

		std::unique_ptr<Renderer> renderer_ = nullptr;
		std::unique_ptr<Window> window_ = nullptr;
	};
} // namespace luster
