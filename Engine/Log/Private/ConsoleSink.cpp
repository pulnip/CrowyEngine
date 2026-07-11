#include <print>
#include <iostream>
#include "ConsoleSink.hpp"

namespace Crowy
{
    void ConsoleSink::Write(const LogMessage& msg){
        std::tm tm;
        auto tp = std::chrono::system_clock::to_time_t(msg.time_point);

        auto& out = msg.level >= LogLevel::Error ? std::cerr : std::clog;

        std::println(out, "[{:%H:%M:%S}][{}][{}]: {}({}:{})",
            msg.time_point, convert(msg.level), msg.category,
            msg.text, msg.location.file_name(), msg.location.line()
        );
    }
}
