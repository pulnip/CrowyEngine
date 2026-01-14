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

if(RENDER_BACKEND STREQUAL "Metal")
    target_compile_definitions(CrowyRenderSample
    PRIVATE
        CROWY_METALRHI
    )
elseif(RENDER_BACKEND STREQUAL "D3D12")
    target_compile_definitions(CrowyRenderSample
    PRIVATE
        CROWY_D3D12RHI
    )
endif()
