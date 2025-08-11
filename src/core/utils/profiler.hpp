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
			: name_(std::move(name)), start_(std::chrono::high_resolution_clock::now())
		{
		}

		~ScopedTimer()
		{
			auto end = std::chrono::high_resolution_clock::now();
			auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
#if LUSTER_PRINT_PROFILING
            spdlog::info("[PROFILE] {}: {} us", name_, us);
#endif
		}

	private:
		std::string name_;
		std::chrono::high_resolution_clock::time_point start_;
	};

#if !defined(LUSTER_ENABLE_PROFILING)
#define LUSTER_ENABLE_PROFILING 1
#endif

	// 控制是否打印 PROFILE 日志（默认不打印）
#if !defined(LUSTER_PRINT_PROFILING)
#define LUSTER_PRINT_PROFILING 0
#endif

#if LUSTER_ENABLE_PROFILING
#define PROFILE_SCOPE(name_literal) ::luster::ScopedTimer _luster_profiler_timer_##__LINE__(name_literal)
#else
#define PROFILE_SCOPE(name_literal) do {} while(0)
#endif
}
