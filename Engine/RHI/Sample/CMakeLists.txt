add_executable(CrowyRHISample)
crowy_link_assets(CrowyRHISample)

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
if(APPLE)
    target_link_libraries(CrowyRHISample
    PRIVATE
        Crowy::MetalRHI
    )
elseif(WIN32)
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

