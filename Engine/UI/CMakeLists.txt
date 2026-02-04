crowy_declare_module(UI)

target_link_libraries(CrowyUI
PUBLIC
    Crowy::CommonRHI
    Crowy::EngineCore
    imgui
PRIVATE
    Crowy::RHI
)

crowy_rhi_macro(UI)
if(RENDER_BACKEND STREQUAL "Metal")
    target_link_libraries(CrowyUI
    PRIVATE
        Crowy::MetalRHI
    )
elseif(RENDER_BACKEND STREQUAL "D3D11")
    target_link_libraries(CrowyUI
    PRIVATE
        Crowy::D3D11RHI
    )
elseif(RENDER_BACKEND STREQUAL "D3D12")
    target_compile_definitions(CrowyUI
    PRIVATE
        CROWY_D3D12RHI
    )
    target_link_libraries(CrowyUI
    PRIVATE
        Crowy::D3D12RHI
    )
endif()