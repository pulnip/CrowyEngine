crowy_declare_module(Platform)

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

if(RENDER_BACKEND STREQUAL "Metal")
    target_link_libraries(CrowyPlatform
    PRIVATE
        Crowy::MetalRHI
    )
elseif(RENDER_BACKEND STREQUAL "D3D12")
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

crowy_rhi_macro(Platform)