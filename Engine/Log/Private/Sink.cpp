#include "Sink.hpp"

#include "Logger.hpp"

namespace Crowy
{
    void AddSink(RAII<Sink> sink) {
        static auto& logger = Logger::Get();

        logger.AddSink(std::move(sink));
    }
}
