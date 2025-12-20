add_library(CrowyLog STATIC)

target_sources(CrowyLog
PRIVATE
    Private/ConsoleSink.cpp
    Private/Logger.cpp
)

target_include_directories(CrowyLog
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

add_library(Crowy::Log ALIAS CrowyLog)