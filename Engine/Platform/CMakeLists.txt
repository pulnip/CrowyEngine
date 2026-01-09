add_library(CrowyPlatform STATIC)

file(GLOB PUBLIC_SOURCES
    "Public/*.hpp"
)
file(GLOB PRIVATE_SOURCES
    "Private/*.hpp"
    "Private/*.cpp"
)

target_sources(CrowyPlatform
PUBLIC
    ${PUBLIC_SOURCES}
PRIVATE
    ${PRIVATE_SOURCES}
)

target_include_directories(CrowyPlatform
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

target_link_libraries(CrowyPlatform
PUBLIC
    Crowy::CommonRHI
PRIVATE
    SDL3::SDL3
    Crowy::Core
    Crowy::Input
    Crowy::Log
    Crowy::RHI
)

if(APPLE)
    target_link_libraries(CrowyPlatform
    PRIVATE
        Crowy::MetalRHI
    )
elseif(WIN32)
    target_link_libraries(CrowyPlatform
    PRIVATE
        Crowy::D3D12RHI
    )
else()
    target_link_libraries(CrowyPlatform
    PRIVATE
        Crowy::NullRHI
    )
endif()

add_library(Crowy::Platform ALIAS CrowyPlatform)
