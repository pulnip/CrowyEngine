#pragma once

#include "Log.hpp"

#undef LOG_TRACE
#undef LOG_DEBUG
#undef LOG_INFO
#undef LOG_WARN
#undef LOG_ERROR
#undef LOG_FATAL

#define LOG_TRACE(fmt, ...) \
    LOG_IMPL(Crowy::LogLevel::Trace, "MetalRHI", fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) \
    LOG_IMPL(Crowy::LogLevel::Debug, "MetalRHI", fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) \
    LOG_IMPL(Crowy::LogLevel::Info, "MetalRHI", fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) \
    LOG_IMPL(Crowy::LogLevel::Warn, "MetalRHI", fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) \
    LOG_IMPL(Crowy::LogLevel::Error, "MetalRHI", fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) \
    LOG_IMPL(Crowy::LogLevel::Fatal, "MetalRHI", fmt, ##__VA_ARGS__)
