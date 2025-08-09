#include "core/application.hpp"
#include "core/renderer.hpp"
#include <exception>
#include <stdexcept>

namespace luster {

static void log_sdl_error(const char *what) {
  const char *err = SDL_GetError();
  spdlog::error("{}: {}", what, (err && *err) ? err : "unknown");
}

Application::Application() = default;
Application::~Application() { cleanup(); }

void Application::init() {
  spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
  spdlog::info("Luster sandbox starting (SDL + Vulkan triangle)...");

#if defined(_WIN32)
  SDL_RegisterApp("Luster", 0, GetModuleHandleW(nullptr));
#endif

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    log_sdl_error("SDL_Init(SDL_INIT_VIDEO) failed");
    throw std::runtime_error("SDL_Init failed");
  }

  constexpr int width = 1280;
  constexpr int height = 720;
  window_ = new Window("Luster (Vulkan)", width, height,
                       SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

  renderer_ = new Renderer();
  renderer_->init(window_->sdl());
}

void Application::mainLoop() {
  bool running = true;
  while (running) {
    bool framebufferResized = false;
    running = window_->pollEvents(framebufferResized);

    if (framebufferResized) {
      try {
        renderer_->recreateSwapchain(window_->sdl());
      } catch (const std::exception &ex) {
        spdlog::error("recreateSwapchain failed: {}", ex.what());
        running = false;
        break;
      }
    }

    if (!renderer_->drawFrame(window_->sdl())) {
      running = false;
      break;
    }

    SDL_Delay(1);
  }
}

void Application::run() {
  init();
  mainLoop();
  cleanup();
}

void Application::cleanup() {
  if (renderer_) {
    delete renderer_;
    renderer_ = nullptr;
  }

  if (window_) {
    delete window_;
    window_ = nullptr;
  }

  SDL_Quit();

  spdlog::info("Luster sandbox exiting.");
}

} // namespace luster
