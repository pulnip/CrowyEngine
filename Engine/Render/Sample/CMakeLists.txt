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
    imgui
    Crowy::Log
    Crowy::Render
    Crowy::Resource
)
if(RENDER_BACKEND STREQUAL "Metal")
    target_link_libraries(CrowyRenderSample
    PRIVATE
        Crowy::MetalRHI
    )
elseif(RENDER_BACKEND STREQUAL "D3D12")
    target_link_libraries(CrowyRenderSample
    PRIVATE
        Crowy::D3D12RHI
    )
else()
    target_link_libraries(CrowyRenderSample
    PRIVATE
        Crowy::NullRHI
    )
endif()

