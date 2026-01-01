add_library(CrowyD3D12RHI STATIC)

file(GLOB PUBLIC_SOURCES
    "Public/*.hpp"
)
file(GLOB PRIVATE_SOURCES
    "Private/*.hpp"
    "Private/*.cpp"
)

target_sources(CrowyD3D12RHI
PUBLIC
    ${PUBLIC_SOURCES}
PRIVATE
    ${PRIVATE_SOURCES}
)

target_include_directories(CrowyD3D12RHI
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)

if(CROWY_USE_STATIC_RHI)
    target_compile_definitions(CrowyD3D12RHI PUBLIC USE_STATIC_RHI)
else()
    target_link_libraries(CrowyD3D12RHI PUBLIC Crowy::RHI)
endif()

target_link_libraries(CrowyD3D12RHI
PUBLIC
    Crowy::CommonRHI
)

# D3D12 specific libraries (Windows only)
if(WIN32)
    target_link_libraries(CrowyD3D12RHI
    PRIVATE
        d3d12.lib
        dxgi.lib
        d3dcompiler.lib
    )
endif()

add_library(Crowy::D3D12RHI ALIAS CrowyD3D12RHI)