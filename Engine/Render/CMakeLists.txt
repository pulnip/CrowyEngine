add_library(CrowyRender STATIC)

target_sources(CrowyRender
PRIVATE
    Private/RenderPass.cpp
)

target_include_directories(CrowyRender
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

target_link_libraries(CrowyRender
PRIVATE
    Crowy::RHI
    Crowy::ECS
)

add_library(Crowy::Render ALIAS CrowyRender)
