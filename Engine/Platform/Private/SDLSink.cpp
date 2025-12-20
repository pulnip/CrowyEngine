#include <SDL3/SDL_log.h>
#include "SDLSink.hpp"

using Crowy::LogLevel;

namespace{
    SDL_LogPriority toSdlPriority(LogLevel level){
        switch(level){
        case LogLevel::Trace: return SDL_LOG_PRIORITY_TRACE;
        case LogLevel::Debug: return SDL_LOG_PRIORITY_DEBUG;
        case LogLevel::Info:  return SDL_LOG_PRIORITY_INFO;
        case LogLevel::Warn:  return SDL_LOG_PRIORITY_WARN;
        case LogLevel::Error: return SDL_LOG_PRIORITY_ERROR;
        case LogLevel::Fatal: return SDL_LOG_PRIORITY_CRITICAL;
        default:              return SDL_LOG_PRIORITY_INFO;
        }
    }
}

namespace Crowy{
    void SDLSink::write(const LogMessage& msg){
        SDL_LogMessage(
            SDL_LOG_CATEGORY_APPLICATION,
            toSdlPriority(msg.level),
            "[%s] %s(%s:%d)",
            msg.category.name, msg.text.data(),
            msg.location.file_name(), msg.location.line()
        );
    }
}