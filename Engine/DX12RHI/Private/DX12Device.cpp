#include <array>
#include <cstdlib>
#include <print>
#include <stdexcept>
#include <vector>
#include <d3dx12/d3dx12_root_signature.h>
#include <Psapi.h>
#include "DescriptorHeapAllocator.hpp"
#include "DX12Allocator.hpp"
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
#include "RHIRetireQueue.hpp"
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

            // validates descriptor-heap indexing and indirect argument reads
            if(std::getenv("CROWY_D3D_GBV") != nullptr){
                COMRAII<ID3D12Debug5> debug5;
                if(SUCCEEDED(debugController.As(&debug5))){
                    debug5->SetEnableGPUBasedValidation(TRUE);
                    debug5->SetEnableAutoName(TRUE);
                }
            }
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
        D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{
            .HighestShaderModel = D3D_SHADER_MODEL_6_8
        };
        CHECK_HRESULT(device.CheckFeatureSupport(
            D3D12_FEATURE_SHADER_MODEL,
            BYTES(shaderModel)
        ), "Unable to check shader model");
        if(shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_6){
            throw std::runtime_error("Shader Model 6.6 not supported");
        }
        // required for SV_StartInstanceLocation
        capabilities.shaderModel6_8 =
            shaderModel.HighestShaderModel >= D3D_SHADER_MODEL_6_8;

        // required for SV_StartInstanceLocation
        D3D12_FEATURE_DATA_D3D12_OPTIONS21 options21{};
        capabilities.extendedCommandInfo = SUCCEEDED(device.CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS21,
            BYTES(options21)
        )) && options21.ExtendedCommandInfoSupported;

        std::println(
            "Shader Model 6.8: {}, Extended command info: {}",
            capabilities.shaderModel6_8,
            capabilities.extendedCommandInfo
        );

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

    // mirrors debug-layer messages to stderr so a headless run
    // (e.g. Tools/smoke_run.ps1) can see the cause in its log
    void CALLBACK debugMessageToStderr(
        D3D12_MESSAGE_CATEGORY,
        D3D12_MESSAGE_SEVERITY severity,
        D3D12_MESSAGE_ID,
        LPCSTR description,
        void*
    ){
        const char* label = severity <= D3D12_MESSAGE_SEVERITY_ERROR ?
            "D3D12 ERROR" : "D3D12 WARNING";
        std::println(stderr, "{}: {}", label, description);
    }

    // With CROWY_D3D_DEBUG_BREAK set, validation errors raise a debug
    // break; without a debugger attached that aborts the process, so a
    // smoke run turns validation errors into a nonzero exit code - the
    // same contract as Metal's MTL_DEBUG_LAYER assert mode.
    // Needs the debug layer, so it only works in debug builds.
    void setupValidationBreak(Crowy::Device& device){
        using namespace Crowy;

        if(std::getenv("CROWY_D3D_DEBUG_BREAK") == nullptr)
            return;

        COMRAII<ID3D12InfoQueue> infoQueue;
        if(FAILED(device.QueryInterface(IID_PPV_ARGS(&infoQueue)))){
            std::println(stderr,
                "CROWY_D3D_DEBUG_BREAK ignored: debug layer is not active"
            );
            return;
        }

        infoQueue->SetBreakOnSeverity(
            D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE
        );
        infoQueue->SetBreakOnSeverity(
            D3D12_MESSAGE_SEVERITY_ERROR, TRUE
        );

        COMRAII<ID3D12InfoQueue1> infoQueue1;
        if(SUCCEEDED(infoQueue.As(&infoQueue1))){
            DWORD cookie = 0;
            infoQueue1->RegisterMessageCallback(
                &debugMessageToStderr,
                D3D12_MESSAGE_CALLBACK_FLAG_NONE,
                nullptr,
                &cookie
            );
        }
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

    // the two command signatures every DrawBatch goes through: a single draw argument each,
    // so no root signature is needed at creation and no root arguments change per draw
    auto createDrawSignature(Crowy::Device& device){
        using namespace Crowy;

        constexpr std::array args{
            D3D12_INDIRECT_ARGUMENT_DESC{
                .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW
            }
        };
        const D3D12_COMMAND_SIGNATURE_DESC desc{
            .ByteStride = sizeof(RHIDrawArgs),
            .NumArgumentDescs = args.size(),
            .pArgumentDescs = args.data(),
            .NodeMask = 0
        };

        CommandSignatureRAII signature = nullptr;
        CHECK_HRESULT(device.CreateCommandSignature(
            &desc,
            nullptr,
            IID_PPV_ARGS(&signature)
        ), "Failed to create draw command signature");

        return signature;
    }

    auto createDrawIndexedSignature(Crowy::Device& device){
        using namespace Crowy;

        constexpr std::array args{
            D3D12_INDIRECT_ARGUMENT_DESC{
                .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED
            }
        };
        const D3D12_COMMAND_SIGNATURE_DESC desc{
            .ByteStride = sizeof(RHIDrawIndexedArgs),
            .NumArgumentDescs = args.size(),
            .pArgumentDescs = args.data(),
            .NodeMask = 0
        };

        CommandSignatureRAII signature = nullptr;
        CHECK_HRESULT(device.CreateCommandSignature(
            &desc,
            nullptr,
            IID_PPV_ARGS(&signature)
        ), "Failed to create draw-indexed command signature");

        return signature;
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

        // descriptor indices retire through here too, so this must outlive
        // every DescriptorHeapAllocator below
        RHIRetireQueue retireQueue;

        DescriptorHeapAllocatorRAII cbvsrvuavHeap = nullptr;
        DescriptorHeapAllocatorRAII rtvHeap = nullptr;
        DescriptorHeapAllocatorRAII dsvHeap = nullptr;
        DescriptorHeapAllocatorRAII samplerHeap = nullptr;

        RootSignatureRAII globalRootSignature;
        CommandSignatureRAII drawSignature;
        CommandSignatureRAII drawIndexedSignature;
        DX12Capabilities dx12Capabilities{
            .gpuUploadHeap = false
        };

        // every resource below is allocated through this, so it has to
        // outlive them - the staging buffer inside uploadRing included
        DX12Allocator allocator;

        RAII<DX12CommandList> uploadCmdList;
        bool uploadRecorded = false;
        UploadRing uploadRing;

        RAII<DX12Fence> frameFence;
        // increased on Submit / SubmitAndPresent
        u64 frameIndex = 0;

    public:
        Impl(DX12Device& dxDevice)
            : factory(::createFactory())
            , device(::createDevice(*factory.Get()))
            , commandQueue(::createCommandQueue(*device.Get()))
            , cbvsrvuavHeap(std::make_unique<DescriptorHeapAllocator>(
                *device.Get(),
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                UINT(65536),
                retireQueue
            ))
            , rtvHeap(std::make_unique<DescriptorHeapAllocator>(
                *device.Get(),
                D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                UINT(64),
                retireQueue
            ))
            , dsvHeap(std::make_unique<DescriptorHeapAllocator>(
                *device.Get(),
                D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
                UINT(32),
                retireQueue
            ))
            , samplerHeap(std::make_unique<DescriptorHeapAllocator>(
                *device.Get(),
                D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
                UINT(64),
                retireQueue
            ))
            , globalRootSignature(::createGlobalRootSignature(*device.Get()))
            , drawSignature(::createDrawSignature(*device.Get()))
            , drawIndexedSignature(::createDrawIndexedSignature(*device.Get()))
            // capabilities are still zeroed here; the allocator only reads
            // them when it allocates, which is after checkDeviceFeature
            , allocator(*device.Get(), dx12Capabilities)
            , frameFence(std::make_unique<DX12Fence>(*device.Get(), 0))
        {
            setupValidationBreak(*device.Get());
            checkAgilitySDK();
            checkDeviceFeature(*device.Get(), dx12Capabilities);
            InitGlobalSession();

            // for prevent dx12Capabilities side-effect
            uploadCmdList = CreateCommandList();

            auto stagingBuffer = CreateBuffer(
                RHIBufferCreateDesc{
                    .size = 1 << 25,
                    .usage = RHIBufferUsage::CopySrc,
                    .location = RHIMemoryLocation::Upload,
                    .cpuAccess = RHICpuAccess::Write,
                    .initialData = nullptr
                }, "staging buffer"
            );
            uploadRing = UploadRing(
                dxDevice,
                std::move(stagingBuffer),
                [this]{ flushUploads(); }
            );
        }

        ~Impl(){
            retireQueue.CollectAll();
        }

        auto CreateFrameScopoe() noexcept{
            return std::make_unique<DX12FrameScope>();
        }

        RAII<DX12Buffer> CreateBuffer(
            const RHIBufferCreateDesc& desc,
            StrView name
        ){
            auto buffer = std::make_unique<DX12Buffer>(
                allocator,
                desc,
                *cbvsrvuavHeap,
                name
            );

            if(desc.initialData != nullptr && desc.cpuAccess == RHICpuAccess::None){
                ensureUploadBegin();

                UploadGpuOnlyBuffer(
                    *uploadCmdList,
                    uploadRing,
                    D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT,
                    *buffer,
                    desc.usage,
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
                allocator,
                desc,
                *cbvsrvuavHeap,
                *rtvHeap,
                *dsvHeap,
                name
            );
            // Notice. RHIMemoryAccess::Transient == RHIMemoryAccess::GPUOnly
            if(!desc.initialData.empty()){
                // RHISubresourceData carries no slice pitch, so the upload
                // helpers only understand 2D subresources
                CROWY_ASSERT(desc.depth == 1,
                    "initial data upload is not supported for 3D textures"
                );
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
                *drawSignature.Get(),
                *drawIndexedSignature.Get(),
                frameIndex,
                *cbvsrvuavHeap,
                *rtvHeap,
                *dsvHeap,
                *samplerHeap
            );
        }

        void Submit(std::span<RHICommandList*> cmdLists){
            // completed-as-of-entry, before this batch's own tag exists
            retireQueue.Collect(GetCompletedFrame());

            executeCommandLists(cmdLists);

            ++frameIndex;
            signalFrame();
            retireQueue.Tag(frameIndex);
            uploadRing.OnSubmit(frameIndex);
        }

        void SubmitAndPresent(
            std::span<RHICommandList*> cmdLists,
            RHISwapchain& swapchain
        ){
            retireQueue.Collect(GetCompletedFrame());

            executeCommandLists(cmdLists);

            static_cast<DX12Swapchain&>(swapchain).Present();

            ++frameIndex;
            signalFrame();
            retireQueue.Tag(frameIndex);
            uploadRing.OnSubmit(frameIndex);
        }

        u64 GetSubmittedFrame() const noexcept{
            return frameIndex;
        }

        u64 GetCompletedFrame() const noexcept{
            return frameFence->GetValue();
        }

        void WaitFrame(u64 value){
            frameFence->WaitCPU(value, 0);
        }

        void WaitIdle(){
            // signal fresh so this also waits on any GPU work queued
            // after the last per-frame signal (e.g. swapchain Present)
            ++frameIndex;
            signalFrame();
            frameFence->WaitCPU(frameIndex, 0);

            // everything up to and including the fresh signal is now done,
            // so this drains the queue rather than leaving stragglers for
            // a Submit that may never come
            retireQueue.Tag(frameIndex);
            retireQueue.Collect(frameIndex);
        }

        void DeferRetire(std::move_only_function<void()> reclaim){
            retireQueue.Defer(std::move(reclaim));
        }
        u64& GetFrameIndexRef() noexcept{
            return frameIndex;
        }


        RHIBufferSlice AllocateTransient(u32 size, u32 align){
            const auto alloc = uploadRing.Allocate(size, align);

            return RHIBufferSlice{
                .buffer = &alloc.buffer,
                .offset = static_cast<u32>(alloc.offset),
                .size = size,
                .cpuPtr = alloc.cpuPtr
            };
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
                // each Upload* helper opens its own copy pass
                uploadCmdList->Begin();
                uploadRecorded = true;
            }
        }

        // The upload ring's escape hatch: the copies holding its space are
        // still sitting in uploadCmdList, so give them a batch of their own
        // and the frame value that comes with it.
        //
        // Called from inside UploadRing::Allocate, which runs before its
        // caller records anything - so what goes out here is strictly the
        // uploads that came before, and the caller still needs an open list.
        void flushUploads(){
            if(!uploadRecorded)
                return;

            Submit(std::span<RHICommandList*>{});
            // creation uploads are load-time work, and draining first is what
            // makes the allocator Begin() is about to reset provably idle
            WaitFrame(frameIndex);

            ensureUploadBegin();
        }

        void executeCommandLists(std::span<RHICommandList*> cmdLists){
            usize recordedUploadCmdListCount = uploadRecorded ? 1 : 0;
            std::vector<ID3D12CommandList*> dxCmdLists(recordedUploadCmdListCount + cmdLists.size());
            if(uploadRecorded){
                // Close completes the upload releases nobody acquired here -
                // the consumers live in the command lists submitted after
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

        void signalFrame(){
            CHECK_HRESULT(commandQueue->Signal(
                frameFence->Get(),
                frameIndex
            ), "Failed to signal fence");
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

    void DX12Device::Submit(std::span<RHICommandList*> cmdLists){
        impl->Submit(cmdLists);
    }

    void DX12Device::SubmitAndPresent(
        std::span<RHICommandList*> cmdLists,
        RHISwapchain& swapchain
    ){
        impl->SubmitAndPresent(cmdLists, swapchain);
    }

    u64 DX12Device::GetSubmittedFrame() const noexcept{
        return impl->GetSubmittedFrame();
    }

    u64 DX12Device::GetCompletedFrame() const noexcept{
        return impl->GetCompletedFrame();
    }

    void DX12Device::WaitFrame(u64 value){
        impl->WaitFrame(value);
    }

    void DX12Device::WaitIdle(){
        impl->WaitIdle();
    }

    void DX12Device::DeferRetire(std::move_only_function<void()> reclaim){
        impl->DeferRetire(std::move(reclaim));
    }

    u64& DX12Device::GetFrameIndexRef() noexcept{
        return impl->GetFrameIndexRef();
    }

    RHIBufferSlice DX12Device::AllocateTransient(u32 size, u32 align){
        return impl->AllocateTransient(size, align);
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
