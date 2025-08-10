#include "core/application.hpp"
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
        spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
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
        renderer_->init(window_->sdl());
    }

    void Application::mainLoop()
    {
        bool running = true;
        bool framebufferResized = false;
        while (running)
        {
            running = window_->pollEvents(framebufferResized);
            if (framebufferResized)
            {
                renderer_->recreateSwapchain(window_->sdl());
                framebufferResized = false;
            }

            if (!renderer_->drawFrame(window_->sdl()))
            {
                // On hard failure, exit the loop
                running = false;
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
