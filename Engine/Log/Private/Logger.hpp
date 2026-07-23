#pragma once

#include <atomic>
#include <vector>
#include "Primitives.hpp"
#include "Sink.hpp"

namespace Crowy
{
    class Logger{
    private:
        using SinkRAII = RAII<Sink>;

        std::vector<SinkRAII> sinks;
        std::atomic<LogLevel> minLevel = LogLevel::Debug;

    public:
        static Logger& Get();

        void AddSink(SinkRAII sink);
        void Log(LogMessage&& msg);

        void SetMinLevel(LogLevel level);
        LogLevel GetMinLevel() const;

    private:
        Logger();
    };
}
