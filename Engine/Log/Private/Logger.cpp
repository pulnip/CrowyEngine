#include "Logger.hpp"

namespace Crowy
{
    Logger& Logger::instance(){
        static Logger logger;
        return logger;
    }

    void Logger::addSink(Logger::SinkPtr sink){
        std::lock_guard lock(mtx);
        sinks.push_back(std::move(sink));
    }

    void Logger::log(LogMessage&& msg){
        if(msg.level < minLevel)
            return;

        std::lock_guard lock(mtx);
        for(auto& s: sinks){
            s->write(msg);
        }
    }
}