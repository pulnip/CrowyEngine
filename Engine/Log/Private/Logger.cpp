#include "ConsoleSink.hpp"
#include "Logger.hpp"

namespace Crowy
{
    Logger& Logger::instance(){
        static Logger logger;
        return logger;
    }

    Logger::Logger(){
        addSink(std::make_unique<ConsoleSink>());
    }

    void Logger::addSink(Logger::SinkPtr sink){
        std::lock_guard lock(mtx);
        sinks.push_back(std::move(sink));
    }

    void Logger::log(LogMessage&& msg){
        std::lock_guard lock(mtx);
        if(msg.level < minLevel)
            return;

        for(auto& s: sinks){
            s->write(msg);
        }
    }
}