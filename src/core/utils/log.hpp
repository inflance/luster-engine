#pragma once

#include "core/core.hpp"
#include <memory>
#include <spdlog/spdlog.h>

namespace luster
{
    class Log
    {
    public:
        static void init();
        static void shutdown();
        static std::shared_ptr<spdlog::logger>& core();

    private:
        static std::shared_ptr<spdlog::logger> coreLogger_;
    };
}


