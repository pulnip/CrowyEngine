crowy_declare_module(DX12RHI)

target_include_directories(CrowyDX12RHI
BEFORE PRIVATE
    "${agilitysdk_SOURCE_DIR}/build/native/include"
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

target_link_libraries(CrowyDX12RHI
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

add_custom_command(TARGET CrowyDX12RHI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${AGILITY_SDK_BIN_DIR}/D3D12Core.dll"
        "${CMAKE_BINARY_DIR}/bin/D3D12Core.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${AGILITY_SDK_BIN_DIR}/d3d12SDKLayers.dll"
        "${CMAKE_BINARY_DIR}/bin/d3d12SDKLayers.dll"
    COMMENT "Copying DirectX Agility SDK DLL"
)

add_custom_command(TARGET CrowyDX12RHI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DXCOMPILER_DLL}" "${DXIL_DLL}"
        "${CMAKE_BINARY_DIR}/bin"
    COMMENT "Copying dxcompiler.dll/dxil.dll"
)

crowy_declare_private_interface(DX12RHI)

# if(CROWY_ENABLE_TEST)
#     add_subdirectory(Sample)
# endif()
