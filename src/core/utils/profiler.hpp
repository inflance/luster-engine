#pragma once

#include <chrono>
#include <string>
#include <utility>
#include <spdlog/spdlog.h>

namespace luster
{
    class ScopedTimer
    {
    public:
        explicit ScopedTimer(std::string name)
            : name_(std::move(name)), start_(std::chrono::high_resolution_clock::now()) {}

        ~ScopedTimer()
        {
            auto end = std::chrono::high_resolution_clock::now();
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
            spdlog::info("[PROFILE] {}: {} us", name_, us);
        }

    private:
        std::string name_;
        std::chrono::high_resolution_clock::time_point start_;
    };

#if !defined(LUSTER_ENABLE_PROFILING)
#define LUSTER_ENABLE_PROFILING 1
#endif

#if LUSTER_ENABLE_PROFILING
#define PROFILE_SCOPE(name_literal) ::luster::ScopedTimer _luster_profiler_timer_##__LINE__(name_literal)
#else
#define PROFILE_SCOPE(name_literal) do {} while(0)
#endif

    // FpsCounter moved to its own header: core/utils/fps_counter.hpp
}


