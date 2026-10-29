#pragma once

#include <array>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include "EnumUtil.hpp"
#include "HashUtil.hpp"
#include "IntMath.hpp"
#include "Primitives.hpp"
#include "RHIFWD.hpp"
#include "StringUtil.hpp"

namespace Crowy
{
    enum class RHIBackend {
        DirectX12 = 1,
        Metal = 2,
        WebGPU = 3,
    };

    // clang-format off
    CROWY_ENUM_BEGIN(RHIBackend)
        CROWY_ENUM_VALUE(DirectX12)
        CROWY_ENUM_VALUE(Metal)
        CROWY_ENUM_VALUE(WebGPU)
    CROWY_ENUM_END()
    // clang-format on

    struct RHICapabilities {
        bool flipTextureV;
        f32 clipSpaceMinZ;
        u32 textureRowPitchAlign = 1;
        u32 textureOffsetAlign = 1;
    };

    enum class RHIMemoryType : u8 {
        GPUOnly = 0,
        CPUWrite = 1,
        CPURead = 2,
        // for TBDR.
        Transient = 3
    };

    struct RHIBufferCreateDesc {
        u32 size;
        RHIMemoryType memory = RHIMemoryType::GPUOnly;
        bool shaderWrite = false;
        const void* initialData = nullptr;
    };

    enum class RHIPrimitiveTopology : u8 {
        PointList = 0,
        LineList = 1,
        LineStrip = 2,
        TriangleList = 3,
        TriangleStrip = 4,
    };

    enum class RHIPixelFormat : u8 {
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
        BC1_UNORM,
        BC1_UNORM_SRGB,
        BC2_UNORM,
        BC2_UNORM_SRGB,
        BC3_UNORM,
        BC3_UNORM_SRGB,
        BC4_UNORM,
        BC4_SNORM,
        BC5_UNORM,
        BC5_SNORM,
        BC6H_UF16,
        BC6H_SF16,
        BC7_UNORM,
        BC7_UNORM_SRGB,
    };

    inline constexpr auto isDepthFormat(RHIPixelFormat format) {
        using enum RHIPixelFormat;

        switch(format) {
        case D16_UNORM:
            [[fallthrough]];
        case D24_UNORM_S8_UINT:
            [[fallthrough]];
        case D32_FLOAT:
            [[fallthrough]];
        case D32_FLOAT_S8_UINT:
            return true;
        default:
            return false;
        }
    }

    inline constexpr auto hasStencil(RHIPixelFormat format) {
        using enum RHIPixelFormat;

        switch(format) {
        case D24_UNORM_S8_UINT:
            [[fallthrough]];
        case D32_FLOAT_S8_UINT:
            return true;
        default:
            return false;
        }
    }

    inline constexpr bool isBlockCompressed(RHIPixelFormat format) {
        using enum RHIPixelFormat;

        return BC1_UNORM <= format && format <= BC7_UNORM_SRGB;
    }

    inline constexpr u32 getBlockDim(RHIPixelFormat format) {
        return isBlockCompressed(format) ? 4 : 1;
    }

    namespace detail
    {
        inline constexpr u32 getBytesPerPixel(RHIPixelFormat format) {
            using enum RHIPixelFormat;

            switch(format) {
            case R8_UNORM:
                [[fallthrough]];
            case R8_SNORM:
                [[fallthrough]];
            case R8_UINT:
                [[fallthrough]];
            case R8_SINT:
                return 1;
            case R16_UNORM:
                [[fallthrough]];
            case R16_SNORM:
                [[fallthrough]];
            case R16_UINT:
                [[fallthrough]];
            case R16_SINT:
                [[fallthrough]];
            case R16_FLOAT:
                [[fallthrough]];
            case RG8_UNORM:
                [[fallthrough]];
            case RG8_SNORM:
                [[fallthrough]];
            case RG8_UINT:
                [[fallthrough]];
            case RG8_SINT:
                return 2;
            case R32_UINT:
                [[fallthrough]];
            case R32_SINT:
                [[fallthrough]];
            case R32_FLOAT:
                [[fallthrough]];
            case RG16_UNORM:
                [[fallthrough]];
            case RG16_SNORM:
                [[fallthrough]];
            case RG16_UINT:
                [[fallthrough]];
            case RG16_SINT:
                [[fallthrough]];
            case RG16_FLOAT:
                [[fallthrough]];
            case RGBA8_UNORM:
                [[fallthrough]];
            case RGBA8_UNORM_SRGB:
                [[fallthrough]];
            case RGBA8_SNORM:
                [[fallthrough]];
            case RGBA8_UINT:
                [[fallthrough]];
            case RGBA8_SINT:
                [[fallthrough]];
            case BGRA8_UNORM:
                [[fallthrough]];
            case BGRA8_UNORM_SRGB:
                return 4;
            case RG32_UINT:
                [[fallthrough]];
            case RG32_SINT:
                [[fallthrough]];
            case RG32_FLOAT:
                [[fallthrough]];
            case RGBA16_UNORM:
                [[fallthrough]];
            case RGBA16_SNORM:
                [[fallthrough]];
            case RGBA16_UINT:
                [[fallthrough]];
            case RGBA16_SINT:
                [[fallthrough]];
            case RGBA16_FLOAT:
                return 8;
            case RGB32_FLOAT:
                return 12;
            case RGBA32_UINT:
                [[fallthrough]];
            case RGBA32_SINT:
                [[fallthrough]];
            case RGBA32_FLOAT:
                return 16;
            case D16_UNORM:
                return 2;
            case D24_UNORM_S8_UINT:
                [[fallthrough]];
            case D32_FLOAT:
                return 4;
            case D32_FLOAT_S8_UINT:
                return 8;
            default:
                std::unreachable();
            }
        }
    }

    inline constexpr u32 getBytesPerBlock(RHIPixelFormat format) {
        using enum RHIPixelFormat;

        switch(format) {
        case BC1_UNORM:
            [[fallthrough]];
        case BC1_UNORM_SRGB:
            [[fallthrough]];
        case BC4_UNORM:
            [[fallthrough]];
        case BC4_SNORM:
            return 8;
        case BC2_UNORM:
            [[fallthrough]];
        case BC2_UNORM_SRGB:
            [[fallthrough]];
        case BC3_UNORM:
            [[fallthrough]];
        case BC3_UNORM_SRGB:
            [[fallthrough]];
        case BC5_UNORM:
            [[fallthrough]];
        case BC5_SNORM:
            [[fallthrough]];
        case BC6H_UF16:
            [[fallthrough]];
        case BC6H_SF16:
            [[fallthrough]];
        case BC7_UNORM:
            [[fallthrough]];
        case BC7_UNORM_SRGB:
            return 16;
        default:
            return detail::getBytesPerPixel(format);
        }
    }

    inline constexpr u32 getRowPitch(RHIPixelFormat format, u32 width) {
        const u32 dim = getBlockDim(format);
        return ceilDiv(width, dim) * getBytesPerBlock(format);
    }

    // a row of a texture copied into a buffer, padded to the backend's rule
    inline constexpr u32 getReadbackRowPitch(
        RHIPixelFormat format,
        u32 width,
        const RHICapabilities& caps
    ) {
        return nextMul(getRowPitch(format, width), caps.textureRowPitchAlign);
    }

    enum class RHITextureUsage : u8 {
        None = 0,
        // view capability
        ShaderResource = 1 << 0,
        ShaderRead = ShaderResource,
        UnorderedAccess = 1 << 1,
        ShaderWrite = UnorderedAccess,
        RenderTarget = 1 << 2,
        DepthStencil = 1 << 3,
        // (D3D12) blit pass capability
        CopySrc = 1 << 4,
        CopyDst = 1 << 5,
    };

    // an edge's release half rides the producer's EndPass, its acquire half the
    // consumer's BeginPass; a half whose opposite sync is None stands alone

    enum class RHIBarrierSync : u32 {
        None = 0,
        All = 1u << 0,
        Draw = 1u << 1,
        VertexShading = 1u << 2,
        PixelShading = 1u << 3,
        DepthStencil = 1u << 4,
        RenderTarget = 1u << 5,
        Compute = 1u << 6,
        Copy = 1u << 7,
        IndexInput = 1u << 8,
        ExecuteIndirect = 1u << 9,
        AllShading = VertexShading | PixelShading | Compute,
    };

    // read accesses combine, a write stands alone
    enum class RHIBarrierAccess : u32 {
        // "every access compatible with the current layout" (D3D12 semantics)
        Common = 0,
        VertexBuffer = 1u << 0,
        IndexBuffer = 1u << 1,
        UniformBuffer = 1u << 2,
        ShaderResource = 1u << 3,
        UnorderedAccess = 1u << 4,
        RenderTarget = 1u << 5,
        DepthWrite = 1u << 6,
        DepthRead = 1u << 7,
        IndirectArgs = 1u << 8,
        CopySrc = 1u << 9,
        CopyDst = 1u << 10,
        NoAccess = 1u << 31,
    };

    enum class RHITextureLayout : u8 {
        Undefined,
        Common,
        RenderTarget,
        DepthWrite,
        DepthRead,
        ShaderResource,
        UnorderedAccess,
        CopySrc,
        CopyDst,
        Present,
    };

    struct RHIBarrierPoint {
        RHIBarrierSync sync;
        RHIBarrierAccess access;
        // not consumed on the buffer path
        RHITextureLayout layout;

        friend bool operator==(const RHIBarrierPoint&, const RHIBarrierPoint&) =
            default;
    };

    inline constexpr u32 AllSubresources = 0xFFFF'FFFF;

    // numMips == 0 makes firstMip a flat subresource index; AllSubresources
    // there, the default, selects the whole resource
    struct RHISubresourceRange {
        u32 firstMip = AllSubresources;
        u32 numMips = 0;
        u32 firstArraySlice = 0;
        u32 numArraySlice = 0;

        friend bool operator==(
            const RHISubresourceRange&,
            const RHISubresourceRange&
        ) = default;
    };

    // both before/after sides stay in the edge: each end consumes its own
    // side and cross-checks the opposite side against the paired half.
    struct RHIBufferBarrier {
        RHIBuffer* buffer;

        RHIBarrierSync syncBefore, syncAfter;
        RHIBarrierAccess accessBefore, accessAfter;
        // an acquire whose producer is in an earlier submission: nothing in
        // this list pairs with it, so it stands alone though syncBefore names
        // real work
        bool crossSubmission = false;

        friend bool operator==(
            const RHIBufferBarrier&,
            const RHIBufferBarrier&
        ) = default;
    };

    struct RHITextureBarrier {
        RHITexture* texture;

        RHIBarrierSync syncBefore, syncAfter;
        RHIBarrierAccess accessBefore, accessAfter;
        RHITextureLayout layoutBefore, layoutAfter;
        RHISubresourceRange range{};
        // only valid when layoutBefore == Undefined
        bool discard = false;
        // see RHIBufferBarrier::crossSubmission
        bool crossSubmission = false;

        friend bool operator==(
            const RHITextureBarrier&,
            const RHITextureBarrier&
        ) = default;
    };

    struct RHIClearDepthStencil {
        f32 depth = 1.0f;
        u8 stencil = 0;
    };

    struct RHISubresourceData {
        const void* data;
        usize rowPitch;
    };
    struct RHISubresourceLayout {
        u64 offset;
        u32 rowPitch;
        u64 rowSize;
        u32 rowCount;
    };

    // a zero size means the whole mip
    struct RHITextureRegion {
        u32 x = 0, y = 0;
        u32 width = 0, height = 0;
    };

    struct RHITextureCreateDesc {
        // 0 for same as screen
        u32 width = 0, height = 0;
        u32 depth = 1;
        u32 mipLevels = 1;
        u32 arraySize = 1;
        RHIPixelFormat format = RHIPixelFormat::RGBA8_UNORM;
        RHITextureUsage usage = RHITextureUsage::None;
        RHIMemoryType memory = RHIMemoryType::GPUOnly;
        // mip + arraySlice
        std::span<const RHISubresourceData> initialData{};
        // ClearColor for optimize (only Valid at D3D12)
        Color clearColor = Colors::Black;
        RHIClearDepthStencil clearDepthStencil{};
    };

    enum class RHIShaderStage : u8 {
        VertexShader,
        FragmentShader,
        ComputeShader,
    };

    enum class RHILoadAction : u8 { Load, Clear, DontCare };

    enum class RHIStoreAction : u8 {
        Store,
        DontCare,
    };

    struct RHIColorAttachment {
        RHITexture* texture = nullptr;
        RHILoadAction loadAction = RHILoadAction::Clear;
        RHIStoreAction storeAction = RHIStoreAction::Store;
        Color clearColor = Colors::Black;
        // the viewport does not follow from this; size it with the same mip
        u32 mipLevel = 0;
    };

    struct RHIDepthAttachment {
        RHITexture* texture = nullptr;
        RHILoadAction loadAction = RHILoadAction::Clear;
        RHIStoreAction storeAction = RHIStoreAction::Store;
        RHIClearDepthStencil clearDepthStencil{};
        u32 mipLevel = 0;
    };

    struct RHIRenderPassDesc {
        std::span<const RHIColorAttachment> colorAttachments;
        std::optional<RHIDepthAttachment> depthAttachment = std::nullopt;
#if CROWY_DEBUG_BUILD
        Str debugName;
#endif
    };

    enum class RHIIndexFormat {
        UInt16,
        UInt32,
    };

    inline constexpr u32 indexSize(RHIIndexFormat format) {
        return format == RHIIndexFormat::UInt16 ? 2 : 4;
    }

    struct RHIIndexBufferView {
        RHIBuffer* buffer = nullptr;
        RHIIndexFormat format = RHIIndexFormat::UInt32;
        // byte offset of index 0; a draw's startIndex adds on top of it
        u32 offset = 0;
    };

    struct RHIViewport {
        f32 x, y;
        f32 width, height;
        f32 minDepth, maxDepth;
    };

    struct RHIScissorRect {
        i32 left, top;
        i32 right, bottom;
    };

    struct RHIShaderDesc {
        std::filesystem::path path;
        Str entryPoint = "main";

        friend bool operator==(const RHIShaderDesc&, const RHIShaderDesc&) =
            default;
    };
}

template<>
struct std::hash<Crowy::RHIShaderDesc> {
    std::size_t operator()(const Crowy::RHIShaderDesc& desc) const noexcept {
        using namespace Crowy;

        return hashAll(desc.path, desc.entryPoint);
    }
};

namespace Crowy
{
    enum class RHICullMode : u8 {
        None,
        Front,
        Back,
    };

    enum class RHIFillMode : u8 {
        Solid,
        Wireframe,
    };

    struct RHIRasterizerState {
        RHIFillMode fillMode = RHIFillMode::Solid;
        RHICullMode cullMode = RHICullMode::Back;
        bool frontCounterClockwise = false;
        i32 depthBias = 0;
        f32 depthBiasClamp = 0.0f;
        f32 slopeScaledDepthBias = 0.0f;
        bool depthClipEnable = true;
        bool multisampleEnable = false;
        bool antialiasedLineEnable = false;

        friend bool operator==(
            const RHIRasterizerState&,
            const RHIRasterizerState&
        ) = default;
    };
}

template<>
struct std::hash<Crowy::RHIRasterizerState> {
    std::size_t operator()(
        const Crowy::RHIRasterizerState& desc
    ) const noexcept {
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

namespace Crowy
{
    enum class RHIComparisonFunc : u8 {
        Never,
        Less,
        Equal,
        LessEqual,
        Greater,
        NotEqual,
        GreaterEqual,
        Always,
    };

    enum class RHIStencilOp : u8 {
        Keep,
        Zero,
        Replace,
        IncrSat,
        DecrSat,
        Invert,
        IncrWrap,
        DecrWrap,
    };

    struct RHIStencilOpDesc {
        RHIComparisonFunc func = RHIComparisonFunc::Always;
        RHIStencilOp stencilFailOp = RHIStencilOp::Keep;
        RHIStencilOp depthFailOp = RHIStencilOp::Keep;
        RHIStencilOp passOp = RHIStencilOp::Keep;

        friend bool operator==(
            const RHIStencilOpDesc&,
            const RHIStencilOpDesc&
        ) noexcept = default;
    };

    struct RHIStencilState {
        u8 readMask = 0xFF;
        u8 writeMask = 0xFF;
        RHIStencilOpDesc frontFace = {};
        RHIStencilOpDesc backFace = {};

        friend bool operator==(const RHIStencilState&, const RHIStencilState&) =
            default;
    };

    struct RHIDepthStencilState {
        RHIPixelFormat format = RHIPixelFormat::D32_FLOAT;
        bool depthWriteEnable = false;
        RHIComparisonFunc depthFunc = RHIComparisonFunc::Less;
        std::optional<RHIStencilState> stencil = std::nullopt;

        friend bool operator==(
            const RHIDepthStencilState&,
            const RHIDepthStencilState&
        ) = default;
    };
}

template<>
struct std::hash<Crowy::RHIStencilOpDesc> {
    std::size_t operator()(const Crowy::RHIStencilOpDesc& desc) const noexcept {
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
struct std::hash<Crowy::RHIStencilState> {
    std::size_t operator()(const Crowy::RHIStencilState& state) const noexcept {
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
struct std::hash<Crowy::RHIDepthStencilState> {
    std::size_t operator()(
        const Crowy::RHIDepthStencilState& state
    ) const noexcept {
        using namespace Crowy;

        std::size_t h =
            hashAll(state.format, state.depthWriteEnable, state.depthFunc);

        return state.stencil.has_value() ? hashAll(h, *state.stencil) : h;
    }
};

namespace Crowy
{
    enum class RHIBlend : u8 {
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

    enum class RHIBlendOp : u8 {
        Add,
        Subtract,
        ReverseSubtract,
        Min,
        Max,
    };

    enum class RHIColorWriteMask : u8 {
        EnableRed = 1 << 0,
        EnableGreen = 1 << 1,
        EnableBlue = 1 << 2,
        EnableColor = EnableRed | EnableGreen | EnableBlue,
        EnableAlpha = 1 << 3,
        EnableAll = EnableColor | EnableAlpha,
    };

    struct RHIRenderTargetBlendState {
        bool blendEnable = false;
        RHIBlend srcBlend = RHIBlend::One;
        RHIBlend dstBlend = RHIBlend::Zero;
        RHIBlendOp blendOp = RHIBlendOp::Add;
        RHIBlend srcBlendAlpha = RHIBlend::One;
        RHIBlend dstBlendAlpha = RHIBlend::Zero;
        RHIBlendOp blendOpAlpha = RHIBlendOp::Add;
        RHIColorWriteMask writeMask = RHIColorWriteMask::EnableAll;

        friend bool operator==(
            const RHIRenderTargetBlendState&,
            const RHIRenderTargetBlendState&
        ) = default;
    };

    inline constexpr u32 MaxRenderTargets = 8;

    struct RHIBlendState {
        bool alphaToCoverageEnable = false;
        bool independentBlendEnable = false;
        std::array<RHIRenderTargetBlendState, MaxRenderTargets> renderTargets;

        friend bool operator==(const RHIBlendState&, const RHIBlendState&) =
            default;
    };
}

template<>
struct std::hash<Crowy::RHIRenderTargetBlendState> {
    std::size_t operator()(
        const Crowy::RHIRenderTargetBlendState& state
    ) const noexcept {
        using namespace Crowy;

        return state.blendEnable ? hashAll(
                                       state.srcBlend,
                                       state.dstBlend,
                                       state.blendOp,
                                       state.srcBlendAlpha,
                                       state.dstBlendAlpha,
                                       state.blendOpAlpha,
                                       state.writeMask
                                   )
                                 : 0;
    }
};

template<>
struct std::hash<Crowy::RHIBlendState> {
    std::size_t operator()(const Crowy::RHIBlendState& state) const noexcept {
        using namespace Crowy;

        std::size_t h =
            hashAll(state.alphaToCoverageEnable, state.independentBlendEnable);

        for(const auto& rtState: state.renderTargets) {
            h = hashAll(h, rtState);
        }

        return h;
    }
};

namespace Crowy
{
    // HLSL shader model; Metal ignores it
    inline constexpr CStr ShaderProfile = "sm_6_8";

    struct RHIGraphicsPipelineStateDesc {
        RHIPrimitiveTopology topology = RHIPrimitiveTopology::TriangleList;
        RHIShaderDesc vertexShader{.entryPoint = "vs_main"};

        RHIRasterizerState rasterizer = {};
        // absent: no fragment stage, and renderTargetCount must be 0
        std::optional<RHIShaderDesc> fragmentShader = std::nullopt;

        std::optional<RHIDepthStencilState> depthStencil = std::nullopt;
        std::optional<RHIBlendState> blend = std::nullopt;

        std::array<RHIPixelFormat, MaxRenderTargets> renderTargetFormats;
        usize renderTargetCount = 1;

        CStr profile = ShaderProfile;

        // linked into every program the pipeline compiles, each exporting a
        // link-time type the program declares extern; Metal links whole
        // programs
        std::vector<std::filesystem::path> linkedModules;

        // Only the first renderTargetCount formats participate,
        // matching what the hash mixes in.
        friend bool operator==(
            const RHIGraphicsPipelineStateDesc& lhs,
            const RHIGraphicsPipelineStateDesc& rhs
        ) noexcept {
            if(lhs.renderTargetCount != rhs.renderTargetCount)
                return false;
            for(usize i = 0; i < lhs.renderTargetCount; ++i) {
                if(lhs.renderTargetFormats[i] != rhs.renderTargetFormats[i])
                    return false;
            }

            return lhs.topology == rhs.topology &&
                   lhs.vertexShader == rhs.vertexShader &&
                   lhs.rasterizer == rhs.rasterizer &&
                   lhs.fragmentShader == rhs.fragmentShader &&
                   lhs.depthStencil == rhs.depthStencil &&
                   lhs.blend == rhs.blend &&
                   sameCStr(lhs.profile, rhs.profile) &&
                   lhs.linkedModules == rhs.linkedModules;
        }
    };

    struct RHIComputePipelineStateDesc {
        RHIShaderDesc computeShader{.entryPoint = "cs_main"};
    };
}

template<>
struct std::hash<Crowy::RHIGraphicsPipelineStateDesc> {
    std::size_t operator()(
        const Crowy::RHIGraphicsPipelineStateDesc& desc
    ) const noexcept {
        using namespace Crowy;

        std::size_t h = hashAll(
            desc.topology,
            desc.vertexShader,
            desc.rasterizer,
            desc.profile ? StrView{desc.profile} : StrView{}
        );

        if(desc.fragmentShader.has_value()) {
            h = hashAll(h, *desc.fragmentShader);
        }
        if(desc.depthStencil.has_value()) {
            h = hashAll(h, *desc.depthStencil);
        }
        if(desc.blend.has_value()) {
            h = hashAll(h, *desc.blend);
        }

        for(usize i = 0; i < desc.renderTargetCount; ++i) {
            h = hashAll(h, desc.renderTargetFormats[i]);
        }
        for(const auto& module: desc.linkedModules) {
            h = hashAll(h, module);
        }

        return h;
    }
};

template<>
struct std::hash<Crowy::RHIComputePipelineStateDesc> {
    std::size_t operator()(
        const Crowy::RHIComputePipelineStateDesc& desc
    ) const noexcept {
        return hashAll(desc.computeShader);
    }
};

namespace Crowy
{
    // hardware-consumed indirect draw arguments;
    struct RHIDrawArgs {
        u32 vertexCount;
        u32 instanceCount = 1;
        u32 firstVertex = 0;
        // drawID; SV_StartInstanceLocation in the VS
        u32 baseInstance = 0;
    };
    static_assert(sizeof(RHIDrawArgs) == 16);

    struct RHIDrawIndexedArgs {
        u32 indexCount;
        u32 instanceCount = 1;
        u32 firstIndex = 0;
        i32 baseVertex = 0;
        // drawID; SV_StartInstanceLocation in the VS
        u32 baseInstance = 0;
    };
    static_assert(sizeof(RHIDrawIndexedArgs) == 20);

    // one ExecuteIndirect submission: every draw sharing a PSO
    struct DrawBatch {
        RHIGraphicsPipelineState* pso = nullptr;
        // RHIDrawArgs[drawCount] at argsOffset
        RHIBuffer* args = nullptr;
        u64 argsOffset = 0;
        u32 drawCount = 0;
    };

    struct DrawBatchIndexed {
        RHIGraphicsPipelineState* pso = nullptr;
        // RHIDrawIndexedArgs[drawCount] at argsOffset
        RHIBuffer* args = nullptr;
        u64 argsOffset = 0;
        u32 drawCount = 0;
        // every draw in the batch reads this one
        RHIIndexBufferView indices{};
    };
}

namespace Crowy
{
    enum class RHIFilter : u8 { Nearest, Linear };

    enum class RHIAddressMode : u8 { Wrap, Clamp, Mirror, Border };

    struct RHISamplerState {
        RHIFilter minFilter = RHIFilter::Linear;
        RHIFilter magFilter = RHIFilter::Linear;
        RHIFilter mipFilter = RHIFilter::Linear;

        RHIAddressMode addressU = RHIAddressMode::Wrap;
        RHIAddressMode addressV = RHIAddressMode::Wrap;
        RHIAddressMode addressW = RHIAddressMode::Wrap;

        f32 mipLODBias = 0.0f;
        f32 minLOD = 0.0f, maxLOD = std::numeric_limits<f32>::max();

        u32 maxAnisotropy = 1;
        RHIComparisonFunc compareFunc = RHIComparisonFunc::Never;
        Color borderColor = Colors::Black;
    };

    inline constexpr RHISamplerState LinearWrapSampler{
        .minFilter = RHIFilter::Linear,
        .magFilter = RHIFilter::Linear,
        .mipFilter = RHIFilter::Linear,
        .addressU = RHIAddressMode::Wrap,
        .addressV = RHIAddressMode::Wrap,
        .addressW = RHIAddressMode::Wrap
    };
    inline constexpr RHISamplerState LinearClampSampler{
        .minFilter = RHIFilter::Linear,
        .magFilter = RHIFilter::Linear,
        .mipFilter = RHIFilter::Linear,
        .addressU = RHIAddressMode::Clamp,
        .addressV = RHIAddressMode::Clamp,
        .addressW = RHIAddressMode::Clamp
    };
    inline constexpr RHISamplerState LinearMirrorSampler{
        .minFilter = RHIFilter::Linear,
        .magFilter = RHIFilter::Linear,
        .mipFilter = RHIFilter::Linear,
        .addressU = RHIAddressMode::Mirror,
        .addressV = RHIAddressMode::Mirror,
        .addressW = RHIAddressMode::Mirror
    };
    inline constexpr RHISamplerState LinearBorderSampler{
        .minFilter = RHIFilter::Linear,
        .magFilter = RHIFilter::Linear,
        .mipFilter = RHIFilter::Linear,
        .addressU = RHIAddressMode::Border,
        .addressV = RHIAddressMode::Border,
        .addressW = RHIAddressMode::Border
    };

    inline constexpr RHISamplerState NearestWrapSampler{
        .minFilter = RHIFilter::Nearest,
        .magFilter = RHIFilter::Nearest,
        .mipFilter = RHIFilter::Nearest,
        .addressU = RHIAddressMode::Wrap,
        .addressV = RHIAddressMode::Wrap,
        .addressW = RHIAddressMode::Wrap
    };
    inline constexpr RHISamplerState NearestClampSampler{
        .minFilter = RHIFilter::Nearest,
        .magFilter = RHIFilter::Nearest,
        .mipFilter = RHIFilter::Nearest,
        .addressU = RHIAddressMode::Clamp,
        .addressV = RHIAddressMode::Clamp,
        .addressW = RHIAddressMode::Clamp
    };
    inline constexpr RHISamplerState NearestMirrorSampler{
        .minFilter = RHIFilter::Nearest,
        .magFilter = RHIFilter::Nearest,
        .mipFilter = RHIFilter::Nearest,
        .addressU = RHIAddressMode::Mirror,
        .addressV = RHIAddressMode::Mirror,
        .addressW = RHIAddressMode::Mirror
    };
    inline constexpr RHISamplerState NearestBorderSampler{
        .minFilter = RHIFilter::Nearest,
        .magFilter = RHIFilter::Nearest,
        .mipFilter = RHIFilter::Nearest,
        .addressU = RHIAddressMode::Border,
        .addressV = RHIAddressMode::Border,
        .addressW = RHIAddressMode::Border
    };

    // order must match Sampler.slang
    inline constexpr std::array StaticSamplers{
        LinearWrapSampler,
        LinearClampSampler,
        LinearMirrorSampler,
        LinearBorderSampler,
        NearestWrapSampler,
        NearestClampSampler,
        NearestMirrorSampler,
        NearestBorderSampler
    };
}

template<>
struct std::hash<Crowy::RHISamplerState> {
    std::size_t operator()(const Crowy::RHISamplerState& desc) const noexcept {
        using namespace Crowy;

        std::size_t h = hashAll(
            desc.minFilter,
            desc.magFilter,
            desc.mipFilter,
            desc.addressU,
            desc.addressV,
            desc.addressW,
            desc.mipLODBias,
            desc.minLOD,
            desc.maxLOD,
            desc.maxAnisotropy,
            desc.compareFunc
        );

        bool anyAddressModeBorder = desc.addressU == RHIAddressMode::Border ||
                                    desc.addressV == RHIAddressMode::Border ||
                                    desc.addressW == RHIAddressMode::Border;

        return anyAddressModeBorder ? hashAll(h, desc.borderColor) : h;
    }
};

namespace Crowy
{
    inline constexpr u32 FramesInFlight = 3;
    inline constexpr u32 UploadStagingBytes = 32u << 20;
    inline constexpr u32 TransientStagingBytes = 32u << 20;

    struct RHISwapchainCreateDesc {
        void* sdlWindow = nullptr;
        RHITextureCreateDesc bufferDesc;
        u32 bufferCount = FramesInFlight;
        bool vsync = true;
    };

    struct RHIBufferViewDesc {
        struct Raw {
            friend bool operator==(const Raw&, const Raw&) = default;
        };
        struct Typed {
            RHIPixelFormat format = RHIPixelFormat::Unknown;

            friend bool operator==(const Typed&, const Typed&) = default;
        };
        struct Structured {
            u32 stride = 0;

            friend bool operator==(const Structured&, const Structured&) =
                default;
        };
        using Config = std::variant<Raw, Typed, Structured>;

        u32 offset = 0, size = 0;
        Config config = Raw{};

        friend bool operator==(
            const RHIBufferViewDesc&,
            const RHIBufferViewDesc&
        ) = default;
    };

    struct RHIBufferSlice {
        RHIBuffer* buffer = nullptr;
        u32 offset = 0;
        u32 size = 0;
        // where to write it from the CPU; null for a device-local slice
        void* cpuPtr = nullptr;

        [[nodiscard]] bool IsValid() const noexcept {
            return buffer != nullptr;
        }
    };

    inline constexpr u32 AllMips = 0xFFFF'FFFF;

    struct RHITextureViewDesc {
        struct Tex2D {
            friend bool operator==(const Tex2D&, const Tex2D&) = default;
        };
        struct TexCube {
            friend bool operator==(const TexCube&, const TexCube&) = default;
        };
        struct Tex3D {
            friend bool operator==(const Tex3D&, const Tex3D&) = default;
        };
        using Config = std::variant<Tex2D, TexCube, Tex3D>;

        RHIPixelFormat format = RHIPixelFormat::Unknown;
        u32 mostDetailedMip = 0;
        // AllMips reads every mip below mostDetailedMip; render target and
        // unordered access views bind exactly one mip and ignore this
        u32 mipCount = AllMips;
        Config config = Tex2D{};

        friend bool operator==(
            const RHITextureViewDesc&,
            const RHITextureViewDesc&
        ) = default;
    };
}

template<>
struct std::hash<Crowy::RHIBufferViewDesc> {
    std::size_t operator()(
        const Crowy::RHIBufferViewDesc& desc
    ) const noexcept {
        using namespace Crowy;

        std::size_t h = hashAll(desc.offset, desc.size, desc.config.index());

        return std::visit(
            [h](const auto& cfg) {
                using T = std::decay_t<decltype(cfg)>;
                if constexpr(std::is_same_v<T, RHIBufferViewDesc::Raw>) {
                    return h;
                }
                if constexpr(std::is_same_v<T, RHIBufferViewDesc::Typed>) {
                    return hashAll(h, cfg.format);
                }
                else if constexpr(
                    std::is_same_v<T, RHIBufferViewDesc::Structured>
                ) {
                    return hashAll(h, cfg.stride);
                }
            },
            desc.config
        );
    }
};

template<>
struct std::hash<Crowy::RHITextureViewDesc> {
    std::size_t operator()(
        const Crowy::RHITextureViewDesc& desc
    ) const noexcept {
        using namespace Crowy;

        std::size_t h = hashAll(
            desc.format,
            desc.mostDetailedMip,
            desc.mipCount,
            desc.config.index()
        );

        return std::visit(
            [h](const auto& cfg) {
                using T = std::decay_t<decltype(cfg)>;
                if constexpr(std::is_same_v<T, RHITextureViewDesc::Tex2D>) {
                    return h;
                }
                if constexpr(std::is_same_v<T, RHITextureViewDesc::TexCube>) {
                    return h;
                }
                if constexpr(std::is_same_v<T, RHITextureViewDesc::Tex3D>) {
                    return h;
                }
            },
            desc.config
        );
    }
};

namespace Crowy
{
    // bindless rendering api
    inline constexpr u32 DirectCbCount = 3;
    // D3D12's root signature holds 64 DWORDs: a root constant costs one per
    // 4 bytes, a direct CBV two (Metal's setBytes allows 4 KB)
    inline constexpr u32 PushConstantBytes = (64 - 2 * DirectCbCount) * 4;
    inline constexpr u32 CbAlign = 256;
}
