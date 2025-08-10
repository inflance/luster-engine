#pragma once

#include <chrono>
#include <string>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace luster
{
    class FpsCounter
    {
    public:
        FpsCounter() = default;
        explicit FpsCounter(std::string label, double reportIntervalMs = 500.0)
            : label_(std::move(label)), reportIntervalMs_(reportIntervalMs)
        {
            lastSampleReport_ = std::chrono::steady_clock::now();
            lastTickReport_ = lastSampleReport_;
        }

        void setLabel(const std::string& label) { label_ = label; }
        void setReportIntervalMs(double ms) { reportIntervalMs_ = ms; }

        void addSampleMs(double frameMs)
        {
            sampleAccumMs_ += frameMs;
            ++sampleCount_;
            const auto now = std::chrono::steady_clock::now();
            const double elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSampleReport_).count();
            if (elapsedMs >= reportIntervalMs_ && sampleCount_ > 0)
            {
                const double avgMs = sampleAccumMs_ / static_cast<double>(sampleCount_);
                const double fps = avgMs > 0.0 ? 1000.0 / avgMs : 0.0;
                spdlog::info("{} {:.2f} ms | {:.1f} FPS", label_.empty() ? "GPU" : label_, avgMs, fps);
                sampleAccumMs_ = 0.0;
                sampleCount_ = 0;
                lastSampleReport_ = now;
            }
        }

        void tick()
        {
            ++tickCount_;
            const auto now = std::chrono::steady_clock::now();
            const double elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTickReport_).count();
            if (elapsedMs >= reportIntervalMs_ && tickCount_ > 0)
            {
                const double fps = 1000.0 * static_cast<double>(tickCount_) / std::max(1.0, elapsedMs);
                spdlog::info("{} {:.1f} FPS", label_.empty() ? "CPU" : label_, fps);
                tickCount_ = 0;
                lastTickReport_ = now;
            }
        }

    private:
        std::string label_{};
        double reportIntervalMs_ = 500.0;
        double sampleAccumMs_ = 0.0;
        int sampleCount_ = 0;
        std::chrono::steady_clock::time_point lastSampleReport_{};
        int tickCount_ = 0;
        std::chrono::steady_clock::time_point lastTickReport_{};
    };
}


