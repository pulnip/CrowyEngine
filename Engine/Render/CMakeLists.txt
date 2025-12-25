add_library(CrowyRender INTERFACE)

target_include_directories(CrowyRender
INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

add_library(Crowy::Render ALIAS CrowyRender)
