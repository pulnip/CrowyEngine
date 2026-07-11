#pragma once

#include <chrono>
#include <source_location>
#include <thread>
#include <utility>
#include "Primitives.hpp"

namespace Crowy
{
    enum class LogLevel: u8{
        Trace = 0,
        Debug = 1,
        Info = 2,
        Warn = 3,
        Error = 4,
        Fatal = 5
    };

    inline CStr convert(LogLevel level) noexcept{
        switch(level){
        case LogLevel::Trace: return "Trace";
        case LogLevel::Debug: return "Debug";
        case LogLevel::Info:  return "Info";
        case LogLevel::Warn:  return "Warn";
        case LogLevel::Error: return "Error";
        case LogLevel::Fatal: return "Fatal";
        default:
            std::unreachable();
        }
    }

    struct LogMessage{
        LogLevel level;
        CStr category;
        Str text;

        std::source_location location;

        std::thread::id thread_id;
        std::chrono::system_clock::time_point time_point;
    };
}
