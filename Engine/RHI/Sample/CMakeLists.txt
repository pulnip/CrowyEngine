add_executable(CrowyRHISample)

file(GLOB SAMPLE_SOURCES
    "*.hpp"
    "*.cpp"
)

target_sources(CrowyRHISample
PRIVATE
    ${SAMPLE_SOURCES}
)

target_link_libraries(CrowyRHISample
PRIVATE
    SDL3::SDL3
    Crowy::Log
    Crowy::Resource
)
crowy_rhi_macro(RHISample)

if(RENDER_BACKEND STREQUAL "Metal")
    target_link_libraries(CrowyRHISample
    PRIVATE
        Crowy::MetalRHI
    )
elseif(RENDER_BACKEND STREQUAL "D3D11")
    target_link_libraries(CrowyRHISample
    PRIVATE
        Crowy::D3D11RHI
    )
elseif(RENDER_BACKEND STREQUAL "D3D12")
    target_link_libraries(CrowyRHISample
    PRIVATE
        Crowy::D3D12RHI
    )
else()
    target_link_libraries(CrowyRHISample
    PRIVATE
        Crowy::NullRHI
    )
endif()

