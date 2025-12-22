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
        static Logger& instance();

        void addSink(SinkPtr sink);

        void setMinLevel(LogLevel level){
            minLevel = level;
        }
        LogLevel getMinLevel() const{
            return minLevel;
        }

        void log(LogMessage&& msg);

    private:
        Logger();

        std::vector<SinkPtr> sinks;
        LogLevel minLevel = LogLevel::Debug;
        std::mutex mtx;
    };
}