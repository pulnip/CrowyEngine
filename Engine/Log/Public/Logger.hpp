#pragma once

#include <memory>
#include <mutex>
#include <vector>
#include "Sink.hpp"

namespace Crowy
{
    class Logger{
        using SinkPtr = std::unique_ptr<Sink>;

    public:
        static Logger& instance() noexcept;

        void addSink(SinkPtr sink) noexcept;

        void setMinLevel(LogLevel level) noexcept{
            minLevel = level;
        }
        LogLevel getMinLevel() const noexcept{
            return minLevel;
        }

        void log(LogMessage&& msg) noexcept;

    private:
        Logger() noexcept;

        std::vector<SinkPtr> sinks;
        LogLevel minLevel = LogLevel::Debug;
        std::mutex mtx;
    };
}