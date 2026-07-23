#include <cstdlib>
#include <print>
#include "Assert.hpp"

namespace Crowy
{
    void detail::assertFail(
        const char* expr,
        std::source_location sl,
        std::string_view msg
    ) noexcept{
        if(msg.empty()){
            std::println(stderr, 
                "Assertion failed: {}\n  {}:{} ({})",
                expr, sl.file_name(), sl.line(), sl.function_name()
            );
        }
        else{
            std::println(stderr,
                "Assertion failed: {}\n  {}:{} ({})\n  {}",
                expr, sl.file_name(), sl.line(), sl.function_name(), msg
            );
        }
        std::abort();
    }
}