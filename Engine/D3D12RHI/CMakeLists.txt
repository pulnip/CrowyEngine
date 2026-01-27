crowy_declare_module(D3D12RHI)

target_precompile_headers(CrowyD3D12RHI
PRIVATE
    <d3d12.h>
    <d3dcompiler.h>
    <dxgi1_6.h>
    <wrl/client.h>
)

if(CROWY_USE_STATIC_RHI)
    target_compile_definitions(CrowyD3D12RHI PUBLIC USE_STATIC_RHI)
else()
    target_link_libraries(CrowyD3D12RHI PUBLIC Crowy::RHI)
endif()

target_link_libraries(CrowyD3D12RHI
PUBLIC
    Crowy::CommonRHI
PRIVATE
    d3d12.lib
    dxgi.lib
    d3dcompiler.lib
    dxguid.lib
    Crowy::Log
)
