#pragma once

#include <chrono>
#include <string>
#include <source_location>
#include <thread>
#include <utility>

namespace Crowy
{
    enum class LogLevel{
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Fatal
    };

    inline const char* levelToString(LogLevel level){
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

    struct LogCategory{
        const char* const name;
    };

    inline constexpr LogCategory LOG_CORE     { "Core"     };
    inline constexpr LogCategory LOG_APP      { "App"      };
    inline constexpr LogCategory LOG_ECS      { "ECS"      };
    inline constexpr LogCategory LOG_METAL    { "Metal"    };
    inline constexpr LogCategory LOG_PLATFORM { "Platform" };
    inline constexpr LogCategory LOG_RENDER   { "Render"   };
    inline constexpr LogCategory LOG_RESOURCE { "Resource" };
    inline constexpr LogCategory LOG_RHI      { "RHI"      };
    inline constexpr LogCategory LOG_SCENE    { "Scene"    };
    inline constexpr LogCategory LOG_SCRIPT   { "Script"   };

    struct LogMessage{
        LogLevel level;
        LogCategory category;
        std::string text;

        std::source_location location;

        std::thread::id thread_id;
        std::chrono::system_clock::time_point time_point;
    };
}