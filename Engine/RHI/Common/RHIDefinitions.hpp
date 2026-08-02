#pragma once

#include <array>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include "HashUtil.hpp"
#include "IntMath.hpp"
#include "Primitives.hpp"
#include "RHIFWD.hpp"
#include "StringUtil.hpp"

namespace Crowy
{
    enum class RHIBackend{
        DirectX12 = 1,
        Metal     = 2,
        WebGPU    = 3,
    };

    struct RHICapabilities{
        bool flipTextureV;
        f32 clipSpaceMinZ;
        u32 textureRowPitchAlign = 1;
        u32 textureOffsetAlign = 1;
    };

    enum class RHIMemoryAccess: u8{
        GPUOnly  = 0,
        CPUWrite = 1,
        CPURead  = 2,
        // for TBDR.
        Transient = 3
    };

    enum class RHIBufferUsage: u16{
        None             = 0,
        // fixed binding
        VertexBuffer     = 1 << 0,
        IndexBuffer      = 1 << 1,
        ConstantBuffer   = 1 << 2,
        // Indirect Draw Argument Buffer
        IndirectArgument = 1 << 3,
        // view capability
        ShaderResource   = 1 << 4,
        ShaderRead       = ShaderResource,
        UnorderedAccess  = 1 << 5,
        ShaderWrite      = UnorderedAccess,
        // (D3D12) blit pass capability
        CopySrc          = 1 << 6,
        CopyDst          = 1 << 7
    };

    struct RHIBufferCreateDesc{
        u32 size;
        RHIBufferUsage usage = RHIBufferUsage::None;
        RHIMemoryAccess access = RHIMemoryAccess::GPUOnly;
        const void* initialData = nullptr;
    };

    enum class RHIPrimitiveTopology: u8{
        PointList     = 0,
        LineList      = 1,
        LineStrip     = 2,
        TriangleList  = 3,
        TriangleStrip = 4,
    };

    enum class RHIPixelFormat: u8{
        Unknown = 0,

        // 8-bit formats
        R8_UNORM,
        R8_SNORM,
        R8_UINT,
        R8_SINT,

        // 16-bit formats
        R16_UNORM,
        R16_SNORM,
        R16_UINT,
        R16_SINT,
        R16_FLOAT,

        RG8_UNORM,
        RG8_SNORM,
        RG8_UINT,
        RG8_SINT,

        // 32-bit formats
        R32_UINT,
        R32_SINT,
        R32_FLOAT,

        RG16_UNORM,
        RG16_SNORM,
        RG16_UINT,
        RG16_SINT,
        RG16_FLOAT,

        RGBA8_UNORM,
        RGBA8_UNORM_SRGB,
        RGBA8_SNORM,
        RGBA8_UINT,
        RGBA8_SINT,

        BGRA8_UNORM,
        BGRA8_UNORM_SRGB,

        // 64-bit formats
        RG32_UINT,
        RG32_SINT,
        RG32_FLOAT,

        // 96-bit formats
        RGB32_FLOAT,

        RGBA16_UNORM,
        RGBA16_SNORM,
        RGBA16_UINT,
        RGBA16_SINT,
        RGBA16_FLOAT,

        // 128-bit formats
        RGBA32_UINT,
        RGBA32_SINT,
        RGBA32_FLOAT,

        // Depth/stencil formats
        D16_UNORM,
        D24_UNORM_S8_UINT,
        D32_FLOAT,
        D32_FLOAT_S8_UINT,

        // Block-compressed formats
        BC1_UNORM, BC1_UNORM_SRGB,
        BC2_UNORM, BC2_UNORM_SRGB,
        BC3_UNORM, BC3_UNORM_SRGB,
        BC4_UNORM, BC4_SNORM,
        BC5_UNORM, BC5_SNORM,
        BC6H_UF16, BC6H_SF16,
        BC7_UNORM, BC7_UNORM_SRGB,
    };

    inline constexpr auto IsDepthFormat(RHIPixelFormat format){
        using enum RHIPixelFormat;

        switch(format){
        case D16_UNORM:         [[fallthrough]];
        case D24_UNORM_S8_UINT: [[fallthrough]];
        case D32_FLOAT:         [[fallthrough]];
        case D32_FLOAT_S8_UINT:
            return true;
        default:
            return false;
        }
    }

    inline constexpr auto HasStencil(RHIPixelFormat format){
        using enum RHIPixelFormat;

        switch(format){
        case D24_UNORM_S8_UINT: [[fallthrough]];
        case D32_FLOAT_S8_UINT:
            return true;
        default:
            return false;
        }
    }

    inline constexpr bool IsBlockCompressed(RHIPixelFormat format){
        using enum RHIPixelFormat;

        return BC1_UNORM <= format && format <= BC7_UNORM_SRGB;
    }

    inline constexpr u32 GetBlockDim(RHIPixelFormat format){
        return IsBlockCompressed(format) ? 4 : 1;
    }

    namespace detail{
        inline constexpr u32 GetBytesPerPixel(RHIPixelFormat format){
            using enum RHIPixelFormat;

            switch(format){
            case R8_UNORM:          [[fallthrough]];
            case R8_SNORM:          [[fallthrough]];
            case R8_UINT:           [[fallthrough]];
            case R8_SINT:
                return 1;
            case R16_UNORM:         [[fallthrough]];
            case R16_SNORM:         [[fallthrough]];
            case R16_UINT:          [[fallthrough]];
            case R16_SINT:          [[fallthrough]];
            case R16_FLOAT:         [[fallthrough]];
            case RG8_UNORM:         [[fallthrough]];
            case RG8_SNORM:         [[fallthrough]];
            case RG8_UINT:          [[fallthrough]];
            case RG8_SINT:
                return 2;
            case R32_UINT:          [[fallthrough]];
            case R32_SINT:          [[fallthrough]];
            case R32_FLOAT:         [[fallthrough]];
            case RG16_UNORM:        [[fallthrough]];
            case RG16_SNORM:        [[fallthrough]];
            case RG16_UINT:         [[fallthrough]];
            case RG16_SINT:         [[fallthrough]];
            case RG16_FLOAT:        [[fallthrough]];
            case RGBA8_UNORM:       [[fallthrough]];
            case RGBA8_UNORM_SRGB:  [[fallthrough]];
            case RGBA8_SNORM:       [[fallthrough]];
            case RGBA8_UINT:        [[fallthrough]];
            case RGBA8_SINT:        [[fallthrough]];
            case BGRA8_UNORM:       [[fallthrough]];
            case BGRA8_UNORM_SRGB:
                return 4;
            case RG32_UINT:         [[fallthrough]];
            case RG32_SINT:         [[fallthrough]];
            case RG32_FLOAT:        [[fallthrough]];
            case RGBA16_UNORM:      [[fallthrough]];
            case RGBA16_SNORM:      [[fallthrough]];
            case RGBA16_UINT:       [[fallthrough]];
            case RGBA16_SINT:       [[fallthrough]];
            case RGBA16_FLOAT:
                return 8;
            case RGB32_FLOAT:
                return 12;
            case RGBA32_UINT:       [[fallthrough]];
            case RGBA32_SINT:       [[fallthrough]];
            case RGBA32_FLOAT:
                return 16;
            case D16_UNORM:
                return 2;
            case D24_UNORM_S8_UINT: [[fallthrough]];
            case D32_FLOAT:
                return 4;
            case D32_FLOAT_S8_UINT:
                return 8;
            default:
                std::unreachable();
            }
        }
    }

    inline constexpr u32 GetBytesPerBlock(RHIPixelFormat format){
        using enum RHIPixelFormat;

        switch(format){
        // Compressed format => bytes in 4x4 block
        case BC1_UNORM:         [[fallthrough]];
        case BC1_UNORM_SRGB:    [[fallthrough]];
        case BC4_UNORM:         [[fallthrough]];
        case BC4_SNORM:
            return 8;
        case BC2_UNORM:         [[fallthrough]];
        case BC2_UNORM_SRGB:    [[fallthrough]];
        case BC3_UNORM:         [[fallthrough]];
        case BC3_UNORM_SRGB:    [[fallthrough]];
        case BC5_UNORM:         [[fallthrough]];
        case BC5_SNORM:         [[fallthrough]];
        case BC6H_UF16:         [[fallthrough]];
        case BC6H_SF16:         [[fallthrough]];
        case BC7_UNORM:         [[fallthrough]];
        case BC7_UNORM_SRGB:
            return 16;
        // Uncompressed format => bytes per pixel
        default:
            return detail::GetBytesPerPixel(format);
        }
    }

    inline constexpr u32 GetRowPitch(RHIPixelFormat format, u32 width){
        const u32 dim = GetBlockDim(format);
        return ceilDiv(width, dim) * GetBytesPerBlock(format);
    }

    enum class RHITextureUsage: u8{
        None              = 0,
        // view capability
        ShaderResource  = 1 << 0,
        ShaderRead      = ShaderResource,
        UnorderedAccess = 1 << 1,
        ShaderWrite     = UnorderedAccess,
        RenderTarget    = 1 << 2,
        DepthStencil    = 1 << 3,
        // (D3D12) blit pass capability
        CopySrc         = 1 << 4,
        CopyDst         = 1 << 5,
    };

    // based on D3D12 Enhanced Barrier for hazard tracking
    // Pipeline Stage Scope
    enum class RHIBarrierSync: u32{
        None                 = 0,
        All                  = 1 << 0,
        Draw                 = 1 << 1,
        IndexInput           = 1 << 2,
        Vertex               = 1 << 3,
        Fragment             = 1 << 4,
        DepthStencil         = 1 << 5,
        RenderTarget         = 1 << 6,
        Compute              = 1 << 7,
        Copy                 = 1 << 9,
        Resolve              = 1 << 10,
        ExecuteIndirect      = 1 << 11,
        Predication          = ExecuteIndirect,
        AllShading           = 1 << 12,
        NonFragment          = 1 << 13,
        ClearUnorderedAccess = 1 << 15,
        Split                = 1 << 27
    };

    // Cache Visibility
    enum class RHIBarrierAccess: u32{
        Common           = 0,
        VertexBuffer     = 1 << 0,
        ConstantBuffer   = 1 << 1,
        IndexBuffer      = 1 << 2,
        RenderTarget     = 1 << 3,
        UnorderedAccess  = 1 << 4,
        DepthStencilWrite = 1 << 5,
        DepthStencilRead  = 1 << 6,
        ShaderResource    = 1 << 7,
        StreamOutput      = 1 << 8,
        IndirectArgument  = 1 << 9,
        Predication       = IndirectArgument,
        CopyDst           = 1 << 10,
        CopySrc           = 1 << 11,
        ResolveDst        = 1 << 12,
        ResolveSrc        = 1 << 13,
        ShadingRateSource = 1 << 16,
        NoAccess          = 1 << 27
    };

    // Physical Layout in Memory (Texture Only)
    enum class RHIBarrierLayout: u32{
        Undefined = 0xFFFF'FFFF,
        Common = 0,
        Present = Common,
        GenericRead = 1,
        RenderTarget = 2,
        UnorderedAccess = 3,
        DepthStencilWrite = 4,
        DepthStencilRead = 5,
        ShaderResource = 6,
        CopySrc = 7,
        CopyDst = 8,
        ResolveSrc = 9,
        ResolveDst = 10,
        ShadingRateSource = 11
    };

    struct RHIBufferBarrier{
        RHIBuffer& buffer;

        RHIBarrierSync syncAfter;
        RHIBarrierAccess accessAfter;
    };

    struct RHIBarrierPoint{
        RHIBarrierSync sync;
        RHIBarrierAccess access;
        RHIBarrierLayout layout;

        bool operator==(const RHIBarrierPoint&) const = default;
    };

    inline constexpr u32 RHI_ALL_SUBRESOURCES = 0xFFFF'FFFF;

    // numMips == 0 makes firstMip a flat subresource index instead of a range;
    // RHI_ALL_SUBRESOURCES there selects the whole resource, which is the default.
    struct RHISubresourceRange{
        u32 firstMip = RHI_ALL_SUBRESOURCES;
        u32 numMips = 0;
        u32 firstArraySlice = 0;
        u32 numArraySlice = 0;
    };

    struct RHITextureBarrier{
        RHITexture& texture;

        RHIBarrierPoint point;
        RHISubresourceRange range{};
    };

    struct RHIClearDepthStencil{
        f32 depth = 1.0f;
        u8 stencil = 0;
    };

    struct RHISubresourceData{
        const void* data;
        usize rowPitch;
    };
    struct RHISubresourceLayout{
        u64 offset;
        u32 rowPitch;
        u64 rowSize;
        u32 rowCount;
    };

    // a zero size means the whole mip
    struct RHITextureRegion{
        u32 x = 0, y = 0;
        u32 width = 0, height = 0;
    };

    struct RHITextureCreateDesc{
        // 0 for same as screen
        u32 width = 0, height = 0;
        u32 depth = 1;
        u32 mipLevels = 1;
        u32 arraySize = 1;
        RHIPixelFormat format = RHIPixelFormat::RGBA8_UNORM;
        RHITextureUsage usage = RHITextureUsage::None;
        RHIMemoryAccess access = RHIMemoryAccess::GPUOnly;
        // mip + arraySlice
        std::span<const RHISubresourceData> initialData{};
        // ClearColor for optimize (only Valid at D3D12)
        Color clearColor = Colors::Black;
        RHIClearDepthStencil clearDepthStencil{};
    };

    enum class RHIShaderStage: u8{
        VertexShader,
        FragmentShader,
        ComputeShader,
    };

    enum class RHILoadAction: u8{
        Load,    // Preserve existing contents
        Clear,   // Clear to specified color
        DontCare // Don't care about existing contents
    };

    enum class RHIStoreAction: u8{
        Store,    // Save contents
        DontCare, // Don't care about existing contents
    };

    struct RHIColorAttachment{
        RHITexture* texture = nullptr;
        RHILoadAction loadAction = RHILoadAction::Clear;
        RHIStoreAction storeAction = RHIStoreAction::Store;
        Color clearColor = Colors::Black;
        // the viewport does not follow from this; size it with the same mip
        u32 mipLevel = 0;
    };

    struct RHIDepthAttachment{
        RHITexture* texture = nullptr;
        RHILoadAction loadAction = RHILoadAction::Clear;
        RHIStoreAction storeAction = RHIStoreAction::Store;
        RHIClearDepthStencil clearDepthStencil{};
        u32 mipLevel = 0;
    };

    struct RHIRenderPassDesc{
        std::span<const RHIColorAttachment> colorAttachments;
        std::optional<RHIDepthAttachment> depthAttachment = std::nullopt;
    #if defined(_DEBUG) || !defined(NDEBUG)
        Str debugName;
    #endif
    };

    enum class RHIIndexFormat{
        UInt16,
        UInt32,
    };

    struct RHIViewport{
        f32 x, y;
        f32 width, height;
        f32 minDepth, maxDepth;
    };

    struct RHIScissorRect{
        i32 left, top;
        i32 right, bottom;
    };

    enum class RHIInputClassification: u8{
        PerVertex,
        PerInstance,
    };

    struct RHIVertexElement{
        CStr semanticName = nullptr;
        u32 semanticIndex;
        RHIPixelFormat format;
        u32 inputSlot;
        u32 alignedByteOffset;
        RHIInputClassification classification;
        u32 instanceDataStepRate; // For per-instance data
    };
}

template<>
struct std::hash<Crowy::RHIVertexElement>{
    std::size_t operator()(const Crowy::RHIVertexElement& elm) const noexcept{
        using namespace Crowy;

        return hashAll(
            elm.semanticName,
            elm.semanticIndex,
            elm.format,
            elm.inputSlot,
            elm.alignedByteOffset,
            elm.classification,
            elm.instanceDataStepRate
        );
    }
};

namespace Crowy
{
    struct RHIShaderDesc{
        std::filesystem::path path;
        Str entryPoint = "main";
    };
}

template<>
struct std::hash<Crowy::RHIShaderDesc>{
    std::size_t operator()(const Crowy::RHIShaderDesc& desc) const noexcept{
        using namespace Crowy;

        return hashAll(
            desc.path,
            desc.entryPoint
        );
    }
};

namespace Crowy{
    struct RHILegacyFrontendDesc{
        std::optional<std::span<const RHIVertexElement>> vertexLayout = std::nullopt;
        RHIPrimitiveTopology topology = RHIPrimitiveTopology::TriangleList;

        RHIShaderDesc vertexShader{
            .entryPoint = "vs_main"
        };
    };

    struct RHIMeshFrontendDesc{
        // Amplification Shader
        std::optional<RHIShaderDesc> amplificationShader = std::nullopt;
        RHIShaderDesc meshShader;
    };

    using RHIPreRasterizerDesc = std::variant<
        RHILegacyFrontendDesc,
        RHIMeshFrontendDesc
    >;
}

template<>
struct std::hash<Crowy::RHILegacyFrontendDesc>{
    std::size_t operator()(const Crowy::RHILegacyFrontendDesc& desc) const noexcept{
        using namespace Crowy;

        std::size_t h = 0;

        if(desc.vertexLayout.has_value()){
            for(const auto& elm: *desc.vertexLayout){
                h = hashAll(h, elm);
            }
        }

        return hashAll(
            h,
            desc.topology,
            desc.vertexShader
        );
    }
};

template<>
struct std::hash<Crowy::RHIMeshFrontendDesc>{
    std::size_t operator()(const Crowy::RHIMeshFrontendDesc& desc) const noexcept{
        using namespace Crowy;

        std::size_t h = desc.amplificationShader.has_value() ?
            hashAll(*desc.amplificationShader) :
            0;

        return hashAll(
            h,
            desc.meshShader
        );
    }
};

template<>
struct std::hash<Crowy::RHIPreRasterizerDesc>{
    std::size_t operator()(const Crowy::RHIPreRasterizerDesc& desc) const noexcept{
        using namespace Crowy;

        return std::visit([](const auto& desc){
            return hashAll(desc);
        }, desc);
    }
};

namespace Crowy
{
    enum class RHICullMode: u8{
        None,
        Front,
        Back,
    };

    enum class RHIFillMode: u8{
        Solid,
        Wireframe,
    };

    struct RHIRasterizerState{
        RHIFillMode fillMode = RHIFillMode::Solid;
        RHICullMode cullMode = RHICullMode::Back;
        bool frontCounterClockwise = true;
        i32 depthBias = 0;
        f32 depthBiasClamp = 0.0f;
        f32 slopeScaledDepthBias = 0.0f;
        bool depthClipEnable = true;
        bool multisampleEnable = false;
        bool antialiasedLineEnable = false;
    };
}

template<>
struct std::hash<Crowy::RHIRasterizerState>{
    std::size_t operator()(const Crowy::RHIRasterizerState& desc) const noexcept{
        using namespace Crowy;

        return hashAll(
            desc.fillMode,
            desc.cullMode,
            desc.frontCounterClockwise,
            desc.depthBias,
            desc.depthBiasClamp,
            desc.slopeScaledDepthBias,
            desc.multisampleEnable,
            desc.antialiasedLineEnable
        );
    }
};

namespace Crowy{
    enum class RHIComparisonFunc: u8{
        Never,
        Less,
        Equal,
        LessEqual,
        Greater,
        NotEqual,
        GreaterEqual,
        Always,
    };

    enum class RHIStencilOp: u8{
        Keep,
        Zero,
        Replace,
        IncrSat,
        DecrSat,
        Invert,
        IncrWrap,
        DecrWrap,
    };

    struct RHIStencilOpDesc{
        RHIComparisonFunc func = RHIComparisonFunc::Always;
        RHIStencilOp stencilFailOp = RHIStencilOp::Keep;
        RHIStencilOp depthFailOp = RHIStencilOp::Keep;
        RHIStencilOp passOp = RHIStencilOp::Keep;
    };

    struct RHIStencilState{
        u8 readMask = 0xFF;
        u8 writeMask = 0xFF;
        RHIStencilOpDesc frontFace = {};
        RHIStencilOpDesc backFace = {};
    };

    struct RHIDepthStencilState{
        RHIPixelFormat format = RHIPixelFormat::D32_FLOAT;
        bool depthWriteEnable = false;
        RHIComparisonFunc depthFunc = RHIComparisonFunc::Less;
        std::optional<RHIStencilState> stencil = std::nullopt;
    };
}

template<>
struct std::hash<Crowy::RHIStencilOpDesc>{
    std::size_t operator()(const Crowy::RHIStencilOpDesc& desc) const noexcept{
        using namespace Crowy;

        return hashAll(
            desc.func,
            desc.stencilFailOp,
            desc.depthFailOp,
            desc.passOp
        );
    }
};

template<>
struct std::hash<Crowy::RHIStencilState>{
    std::size_t operator()(const Crowy::RHIStencilState& state) const noexcept{
        using namespace Crowy;

        return hashAll(
            state.readMask,
            state.writeMask,
            state.frontFace,
            state.backFace
        );
    }
};

template<>
struct std::hash<Crowy::RHIDepthStencilState>{
    std::size_t operator()(const Crowy::RHIDepthStencilState& state) const noexcept{
        using namespace Crowy;

        std::size_t h = hashAll(
            state.format,
            state.depthWriteEnable,
            state.depthFunc
        );

        return state.stencil.has_value() ?
            hashAll(h, *state.stencil) :
            h;
    }
};

namespace Crowy
{
    enum class RHIBlend: u8{
        Zero,
        One,
        SrcColor,
        InvSrcColor,
        SrcAlpha,
        InvSrcAlpha,
        DstAlpha,
        InvDstAlpha,
        DstColor,
        InvDstColor,
        SrcAlphaSat,
        BlendFactor,
        InvBlendFactor,
    };

    enum class RHIBlendOp: u8{
        Add,
        Subtract,
        ReverseSubtract,
        Min,
        Max,
    };

    enum class RHIColorWriteMask: u8{
        EnableRed   = 1 << 0,
        EnableGreen = 1 << 1,
        EnableBlue  = 1 << 2,
        EnableColor = EnableRed | EnableGreen | EnableBlue,
        EnableAlpha = 1 << 3,
        EnableAll = EnableColor | EnableAlpha,
    };

    struct RHIRenderTargetBlendState{
        bool blendEnable = false;
        RHIBlend srcBlend = RHIBlend::One;
        RHIBlend dstBlend = RHIBlend::Zero;
        RHIBlendOp blendOp = RHIBlendOp::Add;
        RHIBlend srcBlendAlpha = RHIBlend::One;
        RHIBlend dstBlendAlpha = RHIBlend::Zero;
        RHIBlendOp blendOpAlpha = RHIBlendOp::Add;
        RHIColorWriteMask writeMask = RHIColorWriteMask::EnableAll;
    };

    inline constexpr u32 RHI_MAX_RENDER_TARGETS = 8;

    struct RHIBlendState{
        bool alphaToCoverageEnable = false;
        bool independentBlendEnable = false;
        std::array<RHIRenderTargetBlendState, RHI_MAX_RENDER_TARGETS> renderTargets;
    };
}

template<>
struct std::hash<Crowy::RHIRenderTargetBlendState>{
    std::size_t operator()(const Crowy::RHIRenderTargetBlendState& state) const noexcept{
        using namespace Crowy;

        return state.blendEnable ?
            hashAll(
                state.srcBlend,
                state.dstBlend,
                state.blendOp,
                state.srcBlendAlpha,
                state.dstBlendAlpha,
                state.blendOpAlpha,
                state.writeMask
            ) :
            0;
    }
};

template<>
struct std::hash<Crowy::RHIBlendState>{
    std::size_t operator()(const Crowy::RHIBlendState& state) const noexcept{
        using namespace Crowy;

        std::size_t h = hashAll(
            state.alphaToCoverageEnable,
            state.independentBlendEnable
        );

        for(const auto& rtState: state.renderTargets){
            h = hashAll(h, rtState);
        }

        return h;
    }
};

namespace Crowy
{
    struct RHIGraphicsPipelineStateDesc{
        // Geometry Frontend
        RHIPreRasterizerDesc preRasterizer;

        // Geometry Backend
        RHIRasterizerState rasterizer = {};
        RHIShaderDesc fragmentShader{
            .entryPoint = "fs_main"
        };

        std::optional<RHIDepthStencilState> depthStencil = std::nullopt;
        std::optional<RHIBlendState> blend = std::nullopt;

        std::array<RHIPixelFormat, RHI_MAX_RENDER_TARGETS> renderTargetFormats;
        usize renderTargetCount = 1;

        // HLSL shader model; Metal ignores it
        CStr profile = "sm_6_6";
    };

    struct RHIComputePipelineStateDesc{
        RHIShaderDesc computeShader{
            .entryPoint = "cs_main"
        };
    };
}

template<>
struct std::hash<Crowy::RHIGraphicsPipelineStateDesc>{
    std::size_t operator()(const Crowy::RHIGraphicsPipelineStateDesc& desc) const noexcept{
        using namespace Crowy;

        std::size_t h = hashAll(
            desc.preRasterizer,
            desc.rasterizer,
            desc.fragmentShader,
            StrView{desc.profile}
        );

        if(desc.depthStencil.has_value()){
            h = hashAll(h, *desc.depthStencil);
        }
        if(desc.blend.has_value()){
            h = hashAll(h, *desc.blend);
        }

        for(usize i=0; i<desc.renderTargetCount; ++i){
            h = hashAll(h, desc.renderTargetFormats[i]);
        }

        return h;
    }
};

template<>
struct std::hash<Crowy::RHIComputePipelineStateDesc>{
    std::size_t operator()(const Crowy::RHIComputePipelineStateDesc& desc) const noexcept{
        return hashAll(
            desc.computeShader
        );
    }
};

namespace Crowy
{
    // hardware-consumed indirect draw arguments;
    struct RHIDrawIndexedArgs{
        u32 indexCount;
        u32 instanceCount = 1;
        u32 firstIndex = 0;
        i32 baseVertex = 0;
        // drawID; SV_StartInstanceLocation in the VS
        u32 baseInstance = 0;
    };
    static_assert(sizeof(RHIDrawIndexedArgs) == 20);

    // one ExecuteIndirect submission: every draw sharing a PSO
    struct DrawBatch{
        RHIGraphicsPipelineState* pso = nullptr;
        // RHIDrawIndexedArgs[drawCount] at argsOffset
        RHIBuffer* args = nullptr;
        u64 argsOffset = 0;
        u32 drawCount = 0;
        // reserved for GPU-driven compaction; must stay null for now
        RHIBuffer* countBuffer = nullptr;
    };
}

namespace Crowy
{
    enum class RHIFilter: u8{
        Nearest,
        Linear
    };

    enum class RHIAddressMode: u8{
        Wrap,
        Clamp,
        Mirror,
        Border
    };

    struct RHISamplerState{
        RHIFilter minFilter = RHIFilter::Linear;
        RHIFilter magFilter = RHIFilter::Linear;
        RHIFilter mipFilter = RHIFilter::Linear;

        RHIAddressMode addressU = RHIAddressMode::Wrap;
        RHIAddressMode addressV = RHIAddressMode::Wrap;
        RHIAddressMode addressW = RHIAddressMode::Wrap;

        // mip-map option
        f32 mipLODBias = 0.0f;
        f32 minLOD = 0.0f, maxLOD = std::numeric_limits<f32>::max();

        u32 maxAnisotropy = 1;
        RHIComparisonFunc compareFunc = RHIComparisonFunc::Never;
        Color borderColor = Colors::Black;
    };

    inline constexpr RHISamplerState LINEAR_WRAP_SAMPLER{
        .minFilter = RHIFilter::Linear,
        .magFilter = RHIFilter::Linear,
        .mipFilter = RHIFilter::Linear,
        .addressU = RHIAddressMode::Wrap,
        .addressV = RHIAddressMode::Wrap,
        .addressW = RHIAddressMode::Wrap
    };
    inline constexpr RHISamplerState LINEAR_CLAMP_SAMPLER{
        .minFilter = RHIFilter::Linear,
        .magFilter = RHIFilter::Linear,
        .mipFilter = RHIFilter::Linear,
        .addressU = RHIAddressMode::Clamp,
        .addressV = RHIAddressMode::Clamp,
        .addressW = RHIAddressMode::Clamp
    };
    inline constexpr RHISamplerState LINEAR_MIRROR_SAMPLER{
        .minFilter = RHIFilter::Linear,
        .magFilter = RHIFilter::Linear,
        .mipFilter = RHIFilter::Linear,
        .addressU = RHIAddressMode::Mirror,
        .addressV = RHIAddressMode::Mirror,
        .addressW = RHIAddressMode::Mirror
    };
    inline constexpr RHISamplerState LINEAR_BORDER_SAMPLER{
        .minFilter = RHIFilter::Linear,
        .magFilter = RHIFilter::Linear,
        .mipFilter = RHIFilter::Linear,
        .addressU = RHIAddressMode::Border,
        .addressV = RHIAddressMode::Border,
        .addressW = RHIAddressMode::Border
    };

    inline constexpr RHISamplerState NEAREST_WRAP_SAMPLER{
        .minFilter = RHIFilter::Nearest,
        .magFilter = RHIFilter::Nearest,
        .mipFilter = RHIFilter::Nearest,
        .addressU = RHIAddressMode::Wrap,
        .addressV = RHIAddressMode::Wrap,
        .addressW = RHIAddressMode::Wrap
    };
    inline constexpr RHISamplerState NEAREST_CLAMP_SAMPLER{
        .minFilter = RHIFilter::Nearest,
        .magFilter = RHIFilter::Nearest,
        .mipFilter = RHIFilter::Nearest,
        .addressU = RHIAddressMode::Clamp,
        .addressV = RHIAddressMode::Clamp,
        .addressW = RHIAddressMode::Clamp
    };
    inline constexpr RHISamplerState NEAREST_MIRROR_SAMPLER{
        .minFilter = RHIFilter::Nearest,
        .magFilter = RHIFilter::Nearest,
        .mipFilter = RHIFilter::Nearest,
        .addressU = RHIAddressMode::Mirror,
        .addressV = RHIAddressMode::Mirror,
        .addressW = RHIAddressMode::Mirror
    };
    inline constexpr RHISamplerState NEAREST_BORDER_SAMPLER{
        .minFilter = RHIFilter::Nearest,
        .magFilter = RHIFilter::Nearest,
        .mipFilter = RHIFilter::Nearest,
        .addressU = RHIAddressMode::Border,
        .addressV = RHIAddressMode::Border,
        .addressW = RHIAddressMode::Border
    };

    // the fixed sampler table.
    // order must match Sampler.slang
    inline constexpr std::array RHI_STATIC_SAMPLERS{
        LINEAR_WRAP_SAMPLER,
        LINEAR_CLAMP_SAMPLER,
        LINEAR_MIRROR_SAMPLER,
        LINEAR_BORDER_SAMPLER,
        NEAREST_WRAP_SAMPLER,
        NEAREST_CLAMP_SAMPLER,
        NEAREST_MIRROR_SAMPLER,
        NEAREST_BORDER_SAMPLER
    };
}

template<>
struct std::hash<Crowy::RHISamplerState>{
    std::size_t operator()(const Crowy::RHISamplerState& desc) const noexcept{
        using namespace Crowy;

        std::size_t h = hashAll(
            desc.minFilter,
            desc.magFilter,
            desc.mipFilter,
            desc.addressU,
            desc.addressV,
            desc.addressW,
            desc.mipLODBias,
            desc.minLOD, desc.maxLOD,
            desc.maxAnisotropy,
            desc.compareFunc
        );

        bool anyAddressModeBorder =
            desc.addressU == RHIAddressMode::Border ||
            desc.addressV == RHIAddressMode::Border ||
            desc.addressW == RHIAddressMode::Border;

        return anyAddressModeBorder ?
            hashAll(h, desc.borderColor) :
            h;
    }
};

namespace Crowy
{
    inline constexpr u32 RHI_FRAMES_IN_FLIGHT = 3;

    struct RHISwapchainCreateDesc{
        void* sdlWindow = nullptr;
        RHITextureCreateDesc bufferDesc;
        u32 bufferCount = RHI_FRAMES_IN_FLIGHT; // Triple buffering
        bool vsync = true;                           // VSync enabled by default
        bool allowTearing = false;                   // Variable refresh rate
    };

    struct RHIBufferViewDesc{
        struct Raw{
            bool operator==(const Raw&) const = default;
        };
        struct Typed{
            RHIPixelFormat format = RHIPixelFormat::Unknown;

            bool operator==(const Typed&) const = default;
        };
        struct Structured{
            u32 stride = 0;

            bool operator==(const Structured&) const = default;
        };
        using Config = std::variant<Raw, Typed, Structured>;

        u32 offset = 0, size = 0;
        Config config = Raw{};

        bool operator==(const RHIBufferViewDesc&) const = default;
    };

    inline constexpr u32 RHI_ALL_MIPS = 0xFFFF'FFFF;

    struct RHITextureViewDesc{
        struct Tex2D{
            bool operator==(const Tex2D&) const = default;
        };
        struct TexCube{
            bool operator==(const TexCube&) const = default;
        };
        using Config = std::variant<Tex2D, TexCube>;

        RHIPixelFormat format = RHIPixelFormat::Unknown;
        u32 mostDetailedMip = 0;
        // RHI_ALL_MIPS reads every mip below mostDetailedMip.
        // Render target and unordered access views bind exactly one mip and ignore this.
        u32 mipCount = RHI_ALL_MIPS;
        Config config = Tex2D{};

        bool operator==(const RHITextureViewDesc&) const = default;
    };
}

template<>
struct std::hash<Crowy::RHIBufferViewDesc>{
    std::size_t operator()(const Crowy::RHIBufferViewDesc& desc) const noexcept{
        using namespace Crowy;

        std::size_t h = hashAll(
            desc.offset,
            desc.size,
            desc.config.index()
        );

        return std::visit([h](const auto& cfg){
            using T = std::decay_t<decltype(cfg)>;
            if constexpr(std::is_same_v<T, RHIBufferViewDesc::Raw>){
                return h;
            }
            if constexpr(std::is_same_v<T, RHIBufferViewDesc::Typed>){
                return hashAll(h, cfg.format);
            }
            else if constexpr(std::is_same_v<T, RHIBufferViewDesc::Structured>){
                return hashAll(h, cfg.stride);
            }
        }, desc.config);
    }
};

template<>
struct std::hash<Crowy::RHITextureViewDesc>{
    std::size_t operator()(const Crowy::RHITextureViewDesc& desc) const noexcept{
        using namespace Crowy;

        std::size_t h = hashAll(
            desc.format,
            desc.mostDetailedMip,
            desc.mipCount,
            desc.config.index()
        );

        return std::visit([h](const auto& cfg){
            using T = std::decay_t<decltype(cfg)>;
            if constexpr(std::is_same_v<T, RHITextureViewDesc::Tex2D>){
                return h;
            }
            if constexpr(std::is_same_v<T, RHITextureViewDesc::TexCube>){
                return h;
            }
        }, desc.config);
    }
};

namespace Crowy
{
    struct RHISamplerUse{
        // backend binding index ([[sampler(n)]] on Metal)
        u32 slot = 0;
        // index into RHI_STATIC_SAMPLERS
        u32 samplerIndex = 0;
    };

    // Shader Reflection for bindless model
    struct RHIShaderReflection{
        u64 entryPointIndex = std::numeric_limits<u64>::max();
        // for compute pipeline, (0, 0, 0) for others.
        Size3D threadGroupSize{0, 0, 0};

        std::vector<RHISamplerUse> usedSamplers;
    };
    struct RHIProgramReflection{
        std::unordered_map<Str, u32> nameToSlot;
        StringHashMap<RHIShaderReflection> shaderRefl;
    };
}

namespace Crowy
{
    // bindless rendering api
    inline constexpr u32 RHI_PUSH_CONSTANT_BYTES = 64;
    inline constexpr u32 RHI_NUM_DIRECT_CBS = 3;
    inline constexpr u32 RHI_CB_ALIGN = 256;
}
