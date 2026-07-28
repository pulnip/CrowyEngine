#include <array>
#include <print>
#include <stdexcept>
#include <vector>
#include <d3dx12/d3dx12_root_signature.h>
#include <Psapi.h>
#include "DescriptorHeapAllocator.hpp"
#include "DX12Buffer.hpp"
#include "DX12CommandList.hpp"
#include "DX12Definitions.hpp"
#include "DX12Device.hpp"
#include "DX12Fence.hpp"
#include "DX12FrameScope.hpp"
#include "DX12PipelineState.hpp"
#include "DX12Sampler.hpp"
#include "DX12Swapchain.hpp"
#include "DX12Texture.hpp"
#include "DX12Util.hpp"
#include "RHIUtil.hpp"
#include "RHIShader.hpp"
#include "UploadRing.hpp"

namespace{
    auto createFactory(){
        using namespace Crowy;

        UINT dxgiFactoryFlags = 0;
        FactoryRAII factory = nullptr;

    #if defined(_DEBUG) || !defined(NDEBUG)
        // Enable debug layer
        COMRAII<ID3D12Debug> debugController;
        if(SUCCEEDED(D3D12GetDebugInterface(
            IID_PPV_ARGS(&debugController)
        ))){
            debugController->EnableDebugLayer();
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    #endif

        CHECK_HRESULT(CreateDXGIFactory2(
            dxgiFactoryFlags,
            IID_PPV_ARGS(&factory)
        ), "Failed to create DXGI factory");

        return factory;
    }

    auto createDevice(Crowy::Factory& factory){
        using namespace Crowy;

        AdapterRAII adapter, selectedAdapter;
        SIZE_T maxDedicatedMemory = 0;

        for(UINT i=0; factory.EnumAdapters1(i, &adapter)!=DXGI_ERROR_NOT_FOUND; ++i){
            DXGI_ADAPTER_DESC1 desc;
            if(FAILED(adapter->GetDesc1(&desc)))
                continue;

            if(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                continue;

            if(SUCCEEDED(D3D12CreateDevice(
                adapter.Get(),
                D3D_FEATURE_LEVEL_12_2,
                __uuidof(Device),
                nullptr
            ))){
                if(desc.DedicatedVideoMemory > maxDedicatedMemory){
                    maxDedicatedMemory = desc.DedicatedVideoMemory;
                    selectedAdapter = adapter;
                }
            }
        }

        if(selectedAdapter == nullptr){
            throw std::runtime_error("No compatible DX12 adapter found");
        }

        DeviceRAII device = nullptr;
        CHECK_HRESULT(D3D12CreateDevice(
            selectedAdapter.Get(),
            D3D_FEATURE_LEVEL_12_2,
            IID_PPV_ARGS(&device)
        ), "Failed to create DX12 device");

        return device;
    }

    void checkDeviceFeature(Crowy::Device& device, Crowy::DX12Capabilities& capabilities){
        // Shader Model 6.6 Support
        D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{
            .HighestShaderModel = D3D_SHADER_MODEL_6_6
        };
        CHECK_HRESULT(device.CheckFeatureSupport(
            D3D12_FEATURE_SHADER_MODEL,
            BYTES(shaderModel)
        ), "Unable to check shader model");
        if(shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_6){
            throw std::runtime_error("Shader Model 6.6 not supported");
        }

        // Resource Binding Tier 3 Support
        D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
        CHECK_HRESULT(device.CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS,
            BYTES(options)
        ), "Unable to check feature options");
        if(options.ResourceBindingTier < D3D12_RESOURCE_BINDING_TIER_3){
            throw std::runtime_error("Resource Binding Tier 3 not supported");
        }

        // Enhanced Resource Barrier Support
        D3D12_FEATURE_DATA_D3D12_OPTIONS12 options12{};
        CHECK_HRESULT(device.CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS12,
            BYTES(options12)
        ), "Unable to check feature options12");
        if(!options12.EnhancedBarriersSupported){
            throw std::runtime_error("Enhanced Barrier not supported");
        }

        // ResizableBAR Support
        D3D12_FEATURE_DATA_D3D12_OPTIONS16 options16{};
        auto hr = device.CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS16,
            BYTES(options16)
        );
        capabilities.gpuUploadHeap = SUCCEEDED(hr) && options16.GPUUploadHeapSupported;
    }

    void checkAgilitySDK(){
        using namespace Crowy;

        HMODULE hMods[1024];
        DWORD cbNeeded;
        HANDLE hProcess = GetCurrentProcess();

        if(EnumProcessModules(
            hProcess,
            hMods,
            sizeof(hMods),
            &cbNeeded
        )){
            for(usize i=0; i<(cbNeeded/sizeof(HMODULE)); ++i){
                char szModName[MAX_PATH];
                if(GetModuleFileNameExA(
                    hProcess,
                    hMods[i],
                    szModName,
                    sizeof(szModName)
                )){
                    Str path(szModName);
                    if(path.find("D3D12Core.dll") != std::string::npos){
                        std::println(
                            "D3D12Core.dll loaded from: {}",
                            path
                        );
                    }
                }
            }
        }
    }

    auto createCommandQueue(Crowy::Device& device){
        using namespace Crowy;

        const D3D12_COMMAND_QUEUE_DESC queueDesc{
            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
            .Priority = 0,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
            .NodeMask = 0
        };

        CommandQueueRAII commandQueue = nullptr;
        CHECK_HRESULT(device.CreateCommandQueue(
            &queueDesc,
            IID_PPV_ARGS(&commandQueue)
        ), "Failed to create command queue");

        return commandQueue;
    }
}

namespace{
    auto convert(Crowy::RHIAddressMode mode){
        using enum Crowy::RHIAddressMode;

        switch(mode){
        case Wrap  : return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case Clamp : return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case Mirror: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case Border: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        default:
            std::unreachable();
        }
    }

    auto convert(
        Crowy::RHIFilter min, Crowy::RHIFilter mag, Crowy::RHIFilter mip,
        bool anisotropy, bool comparison
    ){
        using enum Crowy::RHIFilter;

        if(anisotropy)
            return comparison ?
                D3D12_FILTER_COMPARISON_ANISOTROPIC :
                D3D12_FILTER_ANISOTROPIC;

        UINT flags = 0;

        if(mip == Linear) flags |= 0x1;
        if(mag == Linear) flags |= 0x4;
        if(min == Linear) flags |= 0x10;
        if(comparison)               flags |= 0x80;

        return static_cast<D3D12_FILTER>(flags);
    }

    auto convert(
        const Crowy::RHISamplerState& desc,
        UINT shaderRegister,
        UINT registerSpace = 0
    ){
        using namespace Crowy;

        return D3D12_STATIC_SAMPLER_DESC{
            .Filter = convert(
                desc.minFilter, desc.magFilter, desc.mipFilter,
                desc.maxAnisotropy > 1,
                desc.compareFunc != RHIComparisonFunc::Never
            ),
            .AddressU = convert(desc.addressU),
            .AddressV = convert(desc.addressV),
            .AddressW = convert(desc.addressW),
            .MipLODBias = desc.mipLODBias,
            .MaxAnisotropy = desc.maxAnisotropy,
            .ComparisonFunc = convert(desc.compareFunc),
            // Notice. discard RHISamplerState::borderColor
            .BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
            .MinLOD = desc.minLOD,
            .MaxLOD = desc.maxLOD,
            .ShaderRegister = shaderRegister,
            .RegisterSpace = registerSpace,
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        };
    }

    auto createGlobalRootSignature(Crowy::Device& device){
        using namespace Crowy;

        std::array<CD3DX12_ROOT_PARAMETER1, 1 + RHI_NUM_DIRECT_CBS> params;
        params[RootParamPush].InitAsConstants(
            RHI_PUSH_CONSTANT_BYTES / 4,
            0,
            0,
            D3D12_SHADER_VISIBILITY_ALL
        );
        for(u32 i=0; i<RHI_NUM_DIRECT_CBS; ++i){
            params[RootParamCBBase + i].InitAsConstantBufferView(
                1 + i,
                0,
                D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
                D3D12_SHADER_VISIBILITY_ALL
            );
        }

        std::array samplers = {
            convert(LINEAR_WRAP_SAMPLER, 0),
            convert(LINEAR_CLAMP_SAMPLER, 1),
            convert(LINEAR_MIRROR_SAMPLER, 2),
            convert(LINEAR_BORDER_SAMPLER, 3),
            convert(NEAREST_WRAP_SAMPLER, 4),
            convert(NEAREST_CLAMP_SAMPLER, 5),
            convert(NEAREST_MIRROR_SAMPLER, 6),
            convert(NEAREST_BORDER_SAMPLER, 7),
        };

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc;
        desc.Init_1_1(
            static_cast<UINT>(params.size()),
            params.data(),
            static_cast<UINT>(samplers.size()),
            samplers.data(),
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED  // bindless
        );
        BlobRAII blob = nullptr, errorBlob = nullptr;
        if(FAILED(D3DX12SerializeVersionedRootSignature(
            &desc,
            D3D_ROOT_SIGNATURE_VERSION_1_1,
            blob.GetAddressOf(),
            errorBlob.GetAddressOf()
        ))){
            auto cstr = static_cast<CStr>(errorBlob->GetBufferPointer());
            throw std::runtime_error(
                std::format(
                    "Failed to serialize RootSignature: {}",
                    cstr
                )
            );
        }

        RootSignatureRAII rootSignature = nullptr;
        CHECK_HRESULT(device.CreateRootSignature(
            0,
            blob->GetBufferPointer(),
            blob->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature)
        ), "Failed to create RootSignature");

        return rootSignature;
    }
}

namespace Crowy
{
    RHIDeviceRAII CreateDX12Device(){
        return std::make_unique<DX12Device>();
    }

    class DX12Device::Impl{
    private:
        FactoryRAII factory = nullptr;
        DeviceRAII device = nullptr;
        CommandQueueRAII commandQueue = nullptr;
        DescriptorHeapAllocatorRAII cbvsrvuavHeap = nullptr;
        DescriptorHeapAllocatorRAII rtvHeap = nullptr;
        DescriptorHeapAllocatorRAII dsvHeap = nullptr;
        DescriptorHeapAllocatorRAII samplerHeap = nullptr;

        RootSignatureRAII globalRootSignature;
        DX12Capabilities dx12Capabilities{
            .gpuUploadHeap = false
        };

        RAII<DX12CommandList> uploadCmdList;
        bool uploadRecorded = false;
        UploadRing uploadRing;

        // increased by FramePacer
        u64 frameIndex = 0;

    public:
        Impl(DX12Device& dxDevice)
            : factory(::createFactory())
            , device(::createDevice(*factory.Get()))
            , commandQueue(::createCommandQueue(*device.Get()))
            , cbvsrvuavHeap(std::make_unique<DescriptorHeapAllocator>(
                *device.Get(),
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                UINT(65536)
            ))
            , rtvHeap(std::make_unique<DescriptorHeapAllocator>(
                *device.Get(),
                D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                UINT(64)
            ))
            , dsvHeap(std::make_unique<DescriptorHeapAllocator>(
                *device.Get(),
                D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
                UINT(32)
            ))
            , samplerHeap(std::make_unique<DescriptorHeapAllocator>(
                *device.Get(),
                D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
                UINT(64)
            ))
            , globalRootSignature(::createGlobalRootSignature(*device.Get()))
        {
            checkAgilitySDK();
            checkDeviceFeature(*device.Get(), dx12Capabilities);
            InitGlobalSession();

            // for prevent dx12Capabilities side-effect
            uploadCmdList = CreateCommandList();

            auto stagingBuffer = CreateBuffer(
                RHIBufferCreateDesc{
                    .size = 1 << 25,
                    .usage = RHIBufferUsage::CopySrc,
                    .access = RHIMemoryAccess::CPUWrite,
                    .initialData = nullptr
                }, "staging buffer"
            );
            uploadRing = UploadRing(std::move(stagingBuffer));
        }

        ~Impl()= default;

        auto CreateFrameScopoe() noexcept{
            return std::make_unique<DX12FrameScope>();
        }

        RAII<DX12Buffer> CreateBuffer(
            const RHIBufferCreateDesc& desc,
            StrView name
        ){
            auto buffer = std::make_unique<DX12Buffer>(
                *device.Get(),
                desc,
                dx12Capabilities,
                frameIndex,
                *cbvsrvuavHeap,
                name
            );

            if(desc.initialData != nullptr && desc.access == RHIMemoryAccess::GPUOnly){
                ensureUploadBegin();

                UploadGpuOnlyBuffer(
                    *uploadCmdList,
                    uploadRing,
                    D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT,
                    *buffer,
                    RHISubresourceData{
                        .data = desc.initialData,
                        .rowPitch = desc.size
                    }
                );
            }

            return buffer;
        }

        auto CreateTexture(
            const RHITextureCreateDesc& desc,
            StrView name
        ){
            using enum RHIMemoryAccess;

            CROWY_ASSERT(desc.access != CPUWrite && desc.access != CPURead,
                "Use RHIBuffer for CPU-Accessable Resource"
            );

            auto texture = std::make_unique<DX12Texture>(
                *device.Get(),
                desc,
                *cbvsrvuavHeap,
                *rtvHeap,
                *dsvHeap,
                name
            );
            // Notice. RHIMemoryAccess::Transient == RHIMemoryAccess::GPUOnly
            if(!desc.initialData.empty()){
                ensureUploadBegin();

                const usize n = desc.mipLevels * desc.arraySize;
                CROWY_ASSERT(desc.initialData.size() == n);

                std::vector<RHISubresourceLayout> layouts(n);
                const auto totalBytes = QueryUploadLayout(
                    *texture,
                    layouts
                );
                UploadTexture(
                    *uploadCmdList,
                    uploadRing,
                    D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT,
                    *texture,
                    desc.initialData,
                    layouts,
                    totalBytes
                );
            }

            return texture;
        }

        RAII<DX12Sampler> CreateSampler(
            const RHISamplerState& desc
        ){
            return std::make_unique<DX12Sampler>(
                desc,
                *samplerHeap
            );
        }

        RAII<DX12GraphicsPipelineState> CreatePipelineState(
            const RHIGraphicsPipelineStateDesc& desc,
            StrView name
        ){
            if(std::get_if<RHILegacyFrontendDesc>(&desc.preRasterizer)){
                return std::make_unique<DX12GraphicsPipelineState>(
                    *device.Get(),
                    desc,
                    *globalRootSignature.Get(),
                    name
                );
            }
            else{
                throw std::runtime_error("Unimplemented");
            }
        }

        RAII<DX12ComputePipelineState> CreatePipelineState(
            const RHIComputePipelineStateDesc& desc,
            StrView name
        ){
            return std::make_unique<DX12ComputePipelineState>(
                *device.Get(),
                desc,
                *globalRootSignature.Get(),
                name
            );
        }

        RAII<DX12Swapchain> CreateSwapchain(
            const RHISwapchainCreateDesc& desc,
            StrView name
        ){
            return std::make_unique<DX12Swapchain>(
                *commandQueue.Get(),
                *factory.Get(),
                desc,
                *cbvsrvuavHeap,
                *rtvHeap,
                *dsvHeap,
                name
            );
        }

        RAII<DX12CommandList> CreateCommandList(){
            return std::make_unique<DX12CommandList>(
                *device.Get(),
                *commandQueue.Get(),
                *globalRootSignature.Get(),
                frameIndex,
                *cbvsrvuavHeap,
                *rtvHeap,
                *dsvHeap,
                *samplerHeap
            );
        }

        RAII<DX12Fence> CreateFence(u64 initialValue){
            return std::make_unique<DX12Fence>(
                *device.Get(),
                initialValue
            );
        }

        void SignalFence(RHIFence& fence, u64 value){
            auto& dxFence = static_cast<DX12Fence&>(fence);

            CHECK_HRESULT(commandQueue->Signal(
                dxFence.Get(),
                value
            ), "Failed to signal fence");
        }

        void SignalFence(RHIFence& fence){
            SignalFence(fence, ++frameIndex);
        }

        void Submit(
            std::span<RHICommandList*> cmdLists
        ){
            usize recordedUploadCmdListCount = uploadRecorded ? 1 : 0;
            std::vector<ID3D12CommandList*> dxCmdLists(recordedUploadCmdListCount + cmdLists.size());
            if(uploadRecorded){
                uploadCmdList->EndBlit();
                uploadCmdList->Close();
                dxCmdLists[0] = uploadCmdList->Get();

                uploadRecorded = false;
            }

            for(usize i=0; i<cmdLists.size(); ++i){
                auto dxCmdList = static_cast<DX12CommandList*>(cmdLists[i]);
                dxCmdLists[recordedUploadCmdListCount + i] = dxCmdList->Get();
            }

            commandQueue->ExecuteCommandLists(
                dxCmdLists.size(),
                dxCmdLists.data()
            );
        }

        u64& GetFrameIndexRef() noexcept{
            return frameIndex;
        }

        UINT64 QueryUploadLayout(
            DX12Texture& texture,
            std::span<RHISubresourceLayout> out
        ){
            const auto desc = texture.Get()->GetDesc();
            const usize n = out.size();

            // Look-up footprint
            std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> fp(n);
            std::vector<UINT> numRows(n);
            std::vector<UINT64> rowBytes(n);
            UINT64 totalBytes = 0;
            device->GetCopyableFootprints(
                &desc,
                0,
                static_cast<UINT>(n),
                0,
                fp.data(),
                numRows.data(),
                rowBytes.data(),
                &totalBytes
            );

            for(usize s=0; s<n; ++s){
                out[s] = RHISubresourceLayout{
                    .offset = fp[s].Offset,
                    .rowPitch = fp[s].Footprint.RowPitch,
                    .rowSize = rowBytes[s],
                    .rowCount = numRows[s]
                };
            }

            return totalBytes;
        }

    private:
        void ensureUploadBegin(){
            if(!uploadRecorded){
                uploadCmdList->Begin();
                uploadCmdList->BeginBlit();
                uploadRecorded = true;
            }
        }
    };

    DX12Device::DX12Device()
        : impl(*this){}

    DX12Device::~DX12Device() = default;

    RHIFrameScopeRAII DX12Device::CreateFrameScope(){
        return impl->CreateFrameScopoe();
    }

    RHIBufferRAII DX12Device::CreateBuffer(
        const RHIBufferCreateDesc& desc,
        StrView name
    ){
        return impl->CreateBuffer(desc, name);
    }

    RHITextureRAII DX12Device::CreateTexture(
        const RHITextureCreateDesc& desc,
        StrView name
    ){
        return impl->CreateTexture(desc, name);
    }

    RAII<DX12Sampler> DX12Device::CreateSampler(
        const RHISamplerState& desc
    ){
        return impl->CreateSampler(desc);
    }

    RAII<RHIGraphicsPipelineState> DX12Device::CreatePipelineState(
        const RHIGraphicsPipelineStateDesc& desc,
        StrView name
    ){
        return impl->CreatePipelineState(desc, name);
    }

    RAII<RHIComputePipelineState> DX12Device::CreatePipelineState(
        const RHIComputePipelineStateDesc& desc,
        StrView name
    ){
        return impl->CreatePipelineState(desc, name);
    }

    RAII<RHISwapchain> DX12Device::CreateSwapchain(
        const RHISwapchainCreateDesc& desc,
        StrView name
    ){
        return impl->CreateSwapchain(desc, name);
    }

    RAII<RHICommandList> DX12Device::CreateCommandList(){
        return impl->CreateCommandList();
    }

    RHIFenceRAII DX12Device::CreateFence(u64 initialValue){
        return impl->CreateFence(initialValue);
    }

    void DX12Device::SignalFence(RHIFence& fence, u64 value){
        impl->SignalFence(fence, value);
    }

    void DX12Device::Submit(
        std::span<RHICommandList*> cmdLists,
        RHIFence& fence
    ){
        impl->Submit(cmdLists);

        impl->SignalFence(fence);
    }

    void DX12Device::SubmitAndPresent(
        std::span<RHICommandList*> cmdLists,
        RHISwapchain& swapchain,
        RHIFence& fence
    ){
        impl->Submit(cmdLists);

        static_cast<DX12Swapchain&>(swapchain).Present();

        impl->SignalFence(fence);
    }

    u64& DX12Device::GetFrameIndexRef() noexcept{
        return impl->GetFrameIndexRef();
    }

    RHICapabilities DX12Device::GetCapabilities() const noexcept{
        return {
            .flipTextureV = true,
            .clipSpaceMinZ = 0.0f,
            .textureRowPitchAlign = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT,
            .textureOffsetAlign = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT
        };
    }

    UINT64 DX12Device::QueryUploadLayout(
        RHITexture& texture,
        std::span<RHISubresourceLayout> out
    ){
        return impl->QueryUploadLayout(
            static_cast<DX12Texture&>(texture),
            out
        );
    }
}
