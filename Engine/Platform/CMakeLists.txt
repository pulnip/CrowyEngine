add_library(CrowyPlatform STATIC)

target_sources(CrowyPlatform
PUBLIC
    Public/MainLoop.hpp
    Public/OS.hpp
PRIVATE
    Private/OS.cpp
    Private/SDLInputProvider.cpp
    Private/SDLSink.cpp
    Private/SDLInputProvider.hpp
    Private/SDLSink.hpp
)

target_include_directories(CrowyPlatform
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

target_link_libraries(CrowyPlatform
PRIVATE
    Crowy::Core
    Crowy::Input
    Crowy::Log
    SDL3::SDL3
)

if(APPLE)
    target_link_libraries(CrowyPlatform
    PRIVATE
        Crowy::MetalRHI
    )
else()
    target_link_libraries(CrowyPlatform
    PRIVATE
        Crowy::NullRHI
    )
endif()

add_library(Crowy::Platform ALIAS CrowyPlatform)

add_executable(Crowy)
crowy_link_assets(Crowy)

target_sources(Crowy
PRIVATE
    Private/main.cpp
)

target_link_libraries(Crowy
PRIVATE
    Crowy::Main
    Crowy::Platform
)
