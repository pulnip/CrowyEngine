#include "ConsoleSink.hpp"

#include <iostream>
#include <print>

namespace Crowy
{
    void ConsoleSink::Write(const LogMessage& msg) {
        std::tm tm;
        auto tp = std::chrono::system_clock::to_time_t(msg.time_point);

        auto& out = msg.level >= LogLevel::Error ? std::cerr : std::clog;

        std::println(
            out,
            "[{:%H:%M:%S}][{}][{}]: {}({}:{})",
            msg.time_point,
            enumName(msg.level),
            msg.category,
            msg.text,
            msg.location.file_name(),
            msg.location.line()
        );
    }
}
