add_executable(CrowyRenderSample)

file(GLOB SAMPLE_SOURCES
    "*.hpp"
    "*.cpp"
)

target_sources(CrowyRenderSample
PRIVATE
    ${SAMPLE_SOURCES}
)

target_link_libraries(CrowyRenderSample
PRIVATE
    SDL3::SDL3
    Crowy::Log
    Crowy::Render
    Crowy::Resource
    Crowy::RHI
    Crowy::UI
)

crowy_rhi_macro(RenderSample)
