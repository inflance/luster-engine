#include "core/utils/log.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace luster
{
	std::shared_ptr<spdlog::logger> Log::coreLogger_{};

	void Log::init()
	{
		if (coreLogger_) return;

		std::vector<spdlog::sink_ptr> sinks;
		sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
		sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("luster.log", true));
		coreLogger_ = std::make_shared<spdlog::logger>("core", sinks.begin(), sinks.end());
#if defined(LUSTER_LOG_LEVEL)
		coreLogger_->set_level(static_cast<spdlog::level::level_enum>(LUSTER_LOG_LEVEL));
#else
        coreLogger_->set_level(spdlog::level::info);
#endif
		coreLogger_->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
		spdlog::set_default_logger(coreLogger_);
	}

	void Log::shutdown()
	{
		coreLogger_.reset();
		spdlog::shutdown();
	}

	std::shared_ptr<spdlog::logger>& Log::core()
	{
		return coreLogger_;
	}
}
