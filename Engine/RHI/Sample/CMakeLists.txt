add_executable(CrowyRHISample)
crowy_link_folder(CrowyRHISample asset)

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
else()
    target_link_libraries(CrowyRHISample
    PRIVATE
        Crowy::NullRHI
    )
endif()

