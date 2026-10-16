crowy_declare_module(DX12RHI
PUBLIC
    Crowy::CommonRHI
    Crowy::RHI
PRIVATE
    d3d12.lib
    dxgi.lib
    dxguid.lib
    # Process Status API
    # for Check DirectX Agility SDK
    psapi.lib
    SDL3::SDL3
    Crowy::Log
)

target_include_directories(CrowyDX12RHI
BEFORE PRIVATE
    "${AGILITY_SDK_ROOT_DIR}/include"
)

target_precompile_headers(CrowyDX12RHI
PRIVATE
    <d3d12.h>
    <dxgiformat.h>
    <d3dcommon.h>
    <d3d12shader.h>
    <dxcapi.h>
    <dxgi1_6.h>
    <d3dx12/d3dx12_core.h>
    <d3dx12/d3dx12_root_signature.h>
    <wrl/client.h>
)

crowy_declare_private_interface(DX12RHI)

# if(CROWY_ENABLE_TEST)
#     add_subdirectory(Sample)
# endif()
