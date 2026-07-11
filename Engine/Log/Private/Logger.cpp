#include "ConsoleSink.hpp"
#include "Logger.hpp"

namespace Crowy
{
    Logger& Logger::Get(){
        static Logger singleton;
        return singleton;
    }

    void Logger::AddSink(Logger::SinkRAII sink){
        sinks.push_back(std::move(sink));
    }

    void Logger::Log(LogMessage&& msg){
        auto level = minLevel.load(std::memory_order_relaxed);

        if(msg.level < level)
            return;

        for(auto& s: sinks){
            s->Write(msg);
        }
    }

    void Logger::SetMinLevel(LogLevel level){
        minLevel.store(level, std::memory_order_relaxed);
    }

    LogLevel Logger::GetMinLevel() const{
        return minLevel.load(std::memory_order_relaxed);
    }

    Logger::Logger(){
        auto console = std::make_unique<ConsoleSink>();

        AddSink(std::move(console));
    }
}
