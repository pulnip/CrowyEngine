#include "Logger.hpp"
#include "Sink.hpp"

namespace Crowy
{
    void AddSink(RAII<Sink> sink){
        static auto& logger = Logger::Get();

        logger.AddSink(std::move(sink));
    }
}
