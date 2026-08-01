#include <SDL3/SDL_video.h>
#include "DX12Definitions.hpp"
#include "DX12FrameDump.hpp"
#include "DX12Util.hpp"
#include "DX12Swapchain.hpp"
#include "DX12Texture.hpp"

namespace{
    DXGI_FORMAT toPhysicalFormat(Crowy::RHIPixelFormat format){
        using namespace Crowy;
        using enum RHIPixelFormat;

        switch(format){
        case RGBA8_UNORM_SRGB:
            format = RGBA8_UNORM;
            break;
        case BGRA8_UNORM_SRGB:
            format = BGRA8_UNORM;
            break;
        default:
            break;
        }

        return convert(format);
    }
}

namespace Crowy
{
    DX12Swapchain::DX12Swapchain(
        CommandQueue& commandQueue,
        Factory& factory,
        const RHISwapchainCreateDesc& desc,
        DescriptorHeapAllocator& cbvsrvuavHeap,
        DescriptorHeapAllocator& rtvHeap,
        DescriptorHeapAllocator& dsvHeap,
        StrView name
    )
        : RHISwapchain(desc.bufferDesc.format)
        , queue(&commandQueue)
        , vsync(desc.vsync)
        , allowTearing(desc.allowTearing)
        , cbvsrvuavHeap(cbvsrvuavHeap)
        , rtvHeap(rtvHeap)
        , dsvHeap(dsvHeap)
    {
        if(desc.allowTearing){
            CHECK_HRESULT(factory.CheckFeatureSupport(
                DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                BYTES(allowTearing)
            ), "Allow Tearing feature not supported");
        }

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
            .Width = desc.bufferDesc.width,
            .Height = desc.bufferDesc.height,
            .Format = toPhysicalFormat(GetFormat()),
            .Stereo = FALSE,
            // No MSAA for swapchain
            .SampleDesc = {1, 0},
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = desc.bufferCount,
            .Scaling = DXGI_SCALING_STRETCH,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
            .Flags = desc.allowTearing ?
                DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : UINT(0)
        };

        auto sdlWindow = static_cast<SDL_Window*>(desc.sdlWindow);
        auto hWnd = SDL_GetPointerProperty(
            SDL_GetWindowProperties(sdlWindow),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER,
            nullptr
        );
        COMRAII<IDXGISwapChain1> swapchain1 = nullptr;
        CHECK_HRESULT(factory.CreateSwapChainForHwnd(
            &commandQueue,
            static_cast<HWND>(hWnd),
            &swapChainDesc,
            nullptr,
            nullptr,
            swapchain1.GetAddressOf()
        ), "Failed to create Swapchain");
        CHECK_HRESULT(swapchain1.As(
            &swapchain
        ), "Failed to query IDXGISwapChain3");
        createBackBuffers(desc.bufferCount);

    #if defined(_DEBUG) || !defined(NDEBUG)
        if(!name.empty()){
            swapchain->SetPrivateData(
                WKPDID_D3DDebugObjectName,
                static_cast<UINT>(name.length()),
                name.data()
            );
        }
    #endif
    }

    DX12Swapchain::~DX12Swapchain() = default;

    void DX12Swapchain::createBackBuffers(u32 bufferCount){
        backBuffers.clear();
        backBuffers.reserve(bufferCount);
        for(u32 i=0; i<bufferCount; ++i){
            backBuffers.push_back(std::make_unique<DX12Texture>(
                *swapchain.Get(),
                GetFormat(),
                i,
                cbvsrvuavHeap,
                rtvHeap,
                dsvHeap
            ));
        }

        currentBackBufferIndex = swapchain->GetCurrentBackBufferIndex();
    }

    bool DX12Swapchain::AcquireNextImage() noexcept{
        currentBackBufferIndex = swapchain->GetCurrentBackBufferIndex();

        return true;
    }

    void DX12Swapchain::Resize(u32 newWidth, u32 newHeight){
        if(newWidth == 0 || newHeight == 0)
            return;

        DXGI_SWAP_CHAIN_DESC1 desc;
        CHECK_HRESULT(swapchain->GetDesc1(&desc), "Failed to query Swapchain description");

        backBuffers.clear();
        CHECK_HRESULT(swapchain->ResizeBuffers(
            0, newWidth, newHeight,
            DXGI_FORMAT_UNKNOWN, desc.Flags
        ), "Failed to resize Swapchain buffer");

        // recreate back buffer resource
        createBackBuffers(desc.BufferCount);
    }

    u32 DX12Swapchain::GetWidth() const noexcept{
        DXGI_SWAP_CHAIN_DESC1 desc;
        swapchain->GetDesc1(&desc);

        return desc.Width;
    }

    u32 DX12Swapchain::GetHeight() const noexcept{
        DXGI_SWAP_CHAIN_DESC1 desc;
        swapchain->GetDesc1(&desc);

        return desc.Height;
    }

    void DX12Swapchain::Present(){
        DumpFrameIfRequested(
            *queue,
            *static_cast<DX12Texture&>(GetCurrentTexture()).Get()
        );

        UINT syncInterval = vsync ? 1 : 0;
        UINT flags = (!vsync && allowTearing) ?
            DXGI_PRESENT_ALLOW_TEARING : 0;

        CHECK_HRESULT(swapchain->Present(
            syncInterval,
            flags
        ), "Failed to present Swapchain");
    }
}
