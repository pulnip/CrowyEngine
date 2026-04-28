#pragma once

#include <d3d11.h>
#include <wrl/client.h>

namespace Crowy
{
    using Device = ID3D11Device;
    using DeviceContext = ID3D11DeviceContext;
    using Factory = IDXGIFactory2;
    using Adapter = IDXGIAdapter1;
    using Swapchain = IDXGISwapChain1;

    // Pipeline state types
    // Input Assembler Stage
    using InputLayout = ID3D11InputLayout;
    // Vertex Shader Stage
    using VertexShader = ID3D11VertexShader;
    // Rasterizer Stage
    using RasterizerState = ID3D11RasterizerState;
    // Pixel Shader Stage
    using PixelShader = ID3D11PixelShader;
    // Output Merger Stage
    using DepthStencilState = ID3D11DepthStencilState;
    using BlendState = ID3D11BlendState;

    // Resource types
    using Buffer = ID3D11Buffer;
    using Texture = ID3D11Texture2D;
    using Sampler = ID3D11SamplerState;

    // View types
    using SRV = ID3D11ShaderResourceView;
    using RTV = ID3D11RenderTargetView;
    using UAV = ID3D11UnorderedAccessView;
    using DSV = ID3D11DepthStencilView;

    // RAII wrappers for COM interfaces
    template<typename T>
    using RAII = Microsoft::WRL::ComPtr<T>;

    using DeviceRAII = RAII<Device>;
    using DeviceContextRAII = RAII<DeviceContext>;
    using FactoryRAII = RAII<Factory>;
    using AdapterRAII = RAII<Adapter>;
    using SwapchainRAII = RAII<Swapchain>;

    using InputLayoutRAII = RAII<InputLayout>;
    using RasterizerStateRAII = RAII<RasterizerState>;
    using DepthStencilStateRAII = RAII<DepthStencilState>;
    using BlendStateRAII = RAII<BlendState>;
    using VertexShaderRAII = RAII<VertexShader>;
    using PixelShaderRAII = RAII<PixelShader>;

    using BufferRAII = RAII<Buffer>;
    using TextureRAII = RAII<Texture>;
    using SamplerRAII = RAII<Sampler>;

    using SRVRAII = RAII<SRV>;
    using RTVRAII = RAII<RTV>;
    using UAVRAII = RAII<UAV>;
    using DSVRAII = RAII<DSV>;
}