#pragma once

#include <chrono>
#include <source_location>
#include <thread>

#include "EnumUtil.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    enum class LogLevel : u8 {
        Trace = 0,
        Debug = 1,
        Info = 2,
        Warn = 3,
        Error = 4,
        Fatal = 5
    };

    // clang-format off
    CROWY_ENUM_BEGIN(LogLevel)
        CROWY_ENUM_VALUE(Trace)
        CROWY_ENUM_VALUE(Debug)
        CROWY_ENUM_VALUE(Info)
        CROWY_ENUM_VALUE(Warn)
        CROWY_ENUM_VALUE(Error)
        CROWY_ENUM_VALUE(Fatal)
    CROWY_ENUM_END()
    // clang-format on

    struct LogMessage {
        LogLevel level;
        CStr category;
        Str text;

        std::source_location location;

        std::thread::id thread_id;
        std::chrono::system_clock::time_point time_point;
    };
}
