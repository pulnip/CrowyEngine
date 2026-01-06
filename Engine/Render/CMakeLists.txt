add_library(CrowyRender STATIC)

target_sources(CrowyRender
PUBLIC
    Public/Renderer.hpp
    Public/RenderGraph.hpp
    Public/RenderPass.hpp
PRIVATE
    Private/Renderer.cpp
    Private/RenderTargetPool.cpp
    Private/RenderTargetPool.hpp
)

target_include_directories(CrowyRender
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

target_link_libraries(CrowyRender
PUBLIC
    Crowy::EngineCore
PRIVATE
    Crowy::ECS
    Crowy::Log
    Crowy::Resource
    Crowy::RHI
)

add_library(Crowy::Render ALIAS CrowyRender)
