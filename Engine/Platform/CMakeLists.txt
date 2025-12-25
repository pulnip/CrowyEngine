add_library(CrowyPlatform STATIC)

target_sources(CrowyPlatform
PRIVATE
    Private/OS.cpp
    Private/SDLSink.cpp
)

target_include_directories(CrowyPlatform
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

target_link_libraries(CrowyPlatform
PRIVATE
    Crowy::Core
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
