add_library(CrowyPlatform STATIC)

target_sources(CrowyPlatform
PRIVATE
    Private/Window.cpp
)

target_include_directories(CrowyPlatform
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

target_link_libraries(CrowyPlatform
PRIVATE
    SDL3::SDL3
)

add_library(Crowy::Platform ALIAS CrowyPlatform)