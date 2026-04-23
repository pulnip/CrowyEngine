#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <unordered_map>
#include "enum_traits.hpp"
#include "math.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    struct RHICapabilities{
        bool flipTextureV;
        float clipSpaceMinZ;
    };

    enum class RHIMemoryAccess: uint8_t{
        GPUOnly  = 0,
        CPUWrite = 1,
        CPURead  = 2
    };

    enum class RHIBufferUsage: uint16_t{
        None             = 0,
        // fixed binding
        VertexBuffer     = 1 << 0,
        IndexBuffer      = 1 << 1,
        ConstantBuffer   = 1 << 2,
        // view capability
        AllowShaderRead  = 1 << 4,
        AllowShaderWrite = 1 << 5,
        // others
        IndirectArgs     = 1 << 6,
        CopySource       = 1 << 7,
        CopyDest         = 1 << 8
    };

    constexpr auto BUF_AllowShaderRW = combine(
        RHIBufferUsage::AllowShaderRead,
        RHIBufferUsage::AllowShaderWrite
    );

    struct RHIBufferCreateDesc{
        size_t size;
        RHIBufferUsage usage = RHIBufferUsage::None;
        RHIMemoryAccess access = RHIMemoryAccess::GPUOnly;
        const void* initialData = nullptr;
    };

    enum class RHIPrimitiveTopology: uint8_t{
        PointList     = 0,
        LineList      = 1,
        LineStrip     = 2,
        TriangleList  = 3,
        TriangleStrip = 4,
    };

    enum class RHIPixelFormat: uint8_t{
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

        // Compressed formats
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

    enum class RHITextureUsage: uint8_t{
        None              = 0,
        AllowShaderRead   = 1 << 0,
        AllowRenderTarget = 1 << 1,
        AllowDepthStencil = 1 << 2,
        AllowShaderWrite  = 1 << 3,
        CopySource        = 1 << 4,
        CopyDest          = 1 << 5,
    };

    constexpr auto TEX_AllowShaderRW = combine(
        RHITextureUsage::AllowShaderRead,
        RHITextureUsage::AllowShaderWrite
    );

    enum class RHIResourceState: uint16_t{
        Common                    = 0,
        VertexAndConstantBuffer   = 1 << 0,
        IndexBuffer               = 1 << 1,
        RenderTarget              = 1 << 2,
        UnorderedAccess           = 1 << 3,
        DepthWrite                = 1 << 4,
        DepthRead                 = 1 << 5,
        NonFragmentShaderResource = 1 << 6,
        FragmentShaderResource    = 1 << 7,
        StreamOut                 = 1 << 8,
        IndirectArgument          = 1 << 9,
        CopyDest                  = 1 << 10,
        CopySource                = 1 << 11,
        ResolveDest               = 1 << 12,
        ResolveSource             = 1 << 13,
        GenericRead               =(1 << 14),
        AllShaderResource,
        Present,
        Predication
    };

    struct RHIClearColor{
        float r, g, b, a;
    };

    struct RHIClearDepthStencil{
        float depth;
        uint8_t stencil;
    };

    struct RHITextureCreateDesc{
        // 0 for same as screen
        uint32_t width = 0, height = 0;
        uint32_t depth = 1;
        uint32_t mipLevels = 1;
        uint32_t arraySize = 1;
        RHIPixelFormat format = RHIPixelFormat::BGRA8_UNORM;
        RHITextureUsage usage = RHITextureUsage::None;
        RHIMemoryAccess access = RHIMemoryAccess::GPUOnly;
        RHIResourceState initialState = RHIResourceState::Common;
        RHIClearColor clearColor{
            .r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f
        };
        RHIClearDepthStencil clearDepthStencil{
            .depth = 1.0f, .stencil = 0
        };
        const void* initialData = nullptr;
    };

    enum class RHIShaderStage: uint8_t{
        VertexShader,
        FragmentShader,
        ComputeShader,
    };

    struct RHIShaderCreateDesc{
    #ifdef _WIN32
        const wchar_t* file;
    #else
        const char* file; // source or binary file path
    #endif
        const char* entry;
        RHIShaderStage stage;
    };

    enum class RHILoadAction: uint8_t{
        Load,    // Preserve existing contents
        Clear,   // Clear to specified color
        DontCare // Don't care about existing contents
    };

    enum class RHIStoreAction: uint8_t{
        Store,    // Save contents
        DontCare, // Don't care about existing contents
    };

    enum class RHIIndexFormat{
        UInt16,
        UInt32,
    };

    struct RHIViewport{
        float x, y;
        float width, height;
        float minDepth, maxDepth;
    };

    struct RHIScissorRect{
        int32_t left, top;
        int32_t right, bottom;
    };

    enum class RHIInputClassification: uint8_t{
        PerVertex,
        PerInstance,
    };

    // Standard vertex format
    // Matches common 3D model formats (glTF, FBX, OBJ)
    struct Vertex{
        Vec3 position;
        Vec3 normal;
        Vec2 texCoord;
        Vec4 tangent;  // xyz = tangent direction, w = handedness sign

        // Optional: vertex colors, bone weights, etc. can be added later
    };
    static_assert(sizeof(Vertex) == 48, "Vertex should be 48 bytes");
    static_assert(std::is_trivially_copyable_v<Vertex>, "Vertex must be trivially copyable");

    struct RHIVertexElement{
        const char* semanticName = nullptr;
        uint32_t semanticIndex;
        RHIPixelFormat format;
        uint32_t inputSlot;
        uint32_t alignedByteOffset;
        RHIInputClassification classification;
        uint32_t instanceDataStepRate; // For per-instance data
    };

    struct RHIVertexLayout{
        const RHIVertexElement* elements = nullptr;
        uint32_t elementCount = 0;
    };

    constexpr RHIVertexElement DEFAULT_VERTEX_ELEMENTS[] = {
        {
            .semanticName = "POSITION",
            .semanticIndex = 0,
            .format = RHIPixelFormat::RGB32_FLOAT,  // float3 (12 bytes)
            .inputSlot = 0,
            .alignedByteOffset = 0,
            .classification = RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        },
        {
            .semanticName = "NORMAL",
            .semanticIndex = 0,
            .format = RHIPixelFormat::RGB32_FLOAT,  // float3 (12 bytes)
            .inputSlot = 0,
            .alignedByteOffset = 12,
            .classification = RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        },
        {
            .semanticName = "TEXCOORD",
            .semanticIndex = 0,
            .format = RHIPixelFormat::RG32_FLOAT,
            .inputSlot = 0,
            .alignedByteOffset = 24,
            .classification = RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        },
        {
            .semanticName = "TANGENT",
            .semanticIndex = 0,
            .format = RHIPixelFormat::RGBA32_FLOAT,
            .inputSlot = 0,
            .alignedByteOffset = 32,
            .classification = RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        }
    };
    constexpr RHIVertexLayout DEFAULT_VERTEX_LAYOUT{
        .elements = DEFAULT_VERTEX_ELEMENTS,
        .elementCount = sizeof(DEFAULT_VERTEX_ELEMENTS) / sizeof(RHIVertexElement)
    };

    enum class RHICullMode: uint8_t{
        CullNone,
        Front,
        Back,
    };

    enum class RHIFillMode: uint8_t{
        Solid,
        Wireframe,
    };

    struct RHIRasterizerState{
        RHIFillMode fillMode = RHIFillMode::Solid;
        RHICullMode cullMode = RHICullMode::Back;
        bool frontCounterClockwise = true;
        int32_t depthBias = 0;
        float depthBiasClamp = 0.0f;
        float slopeScaledDepthBias = 0.0f;
        bool depthClipEnable = true;
        bool multisampleEnable = false;
        bool antialiasedLineEnable = false;
    };

    enum class RHIComparisonFunc: uint8_t{
        Never,
        Less,
        Equal,
        LessEqual,
        Greater,
        NotEqual,
        GreaterEqual,
        Always,
    };

    enum class RHIStencilOp: uint8_t{
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
        uint8_t readMask = 0xFF;
        uint8_t writeMask = 0xFF;
        RHIStencilOpDesc frontFace = {};
        RHIStencilOpDesc backFace = {};
    };

    struct RHIDepthStencilState{
        RHIPixelFormat format = RHIPixelFormat::D32_FLOAT;
        bool depthWriteEnable = false;
        RHIComparisonFunc depthFunc = RHIComparisonFunc::Less;
        std::optional<RHIStencilState> stencil = std::nullopt;
    };

    enum class RHIBlend: uint8_t{
        Zero,
        One,
        SrcColor,
        InvSrcColor,
        SrcAlpha,
        InvSrcAlpha,
        DestAlpha,
        InvDestAlpha,
        DestColor,
        InvDestColor,
        SrcAlphaSat,
        BlendFactor,
        InvBlendFactor,
    };

    enum class RHIBlendOp: uint8_t{
        Add,
        Subtract,
        ReverseSubtract,
        Min,
        Max,
    };

    struct RHIRenderTargetBlendState{
        bool blendEnable = false;
        RHIBlend srcBlend = RHIBlend::One;
        RHIBlend dstBlend = RHIBlend::Zero;
        RHIBlendOp blendOp = RHIBlendOp::Add;
        RHIBlend srcBlendAlpha = RHIBlend::One;
        RHIBlend dstBlendAlpha = RHIBlend::Zero;
        RHIBlendOp blendOpAlpha = RHIBlendOp::Add;
        uint8_t renderTargetWriteMask = 0b1111; // RGBA
    };

    struct RHIBlendState{
        bool alphaToCoverageEnable = false;
        bool independentBlendEnable = false;
        RHIRenderTargetBlendState renderTargets[8];
    };

    constexpr auto RHI_MAX_RENDER_TARGETS = 8;

    struct RHIGraphicsPipelineStateDesc{
        RHIShader* vertexShader = nullptr;
        RHIShader* pixelShader = nullptr;

        RHIVertexLayout vertexLayout = DEFAULT_VERTEX_LAYOUT;
        RHIPrimitiveTopology topology = RHIPrimitiveTopology::TriangleList;

        RHIRasterizerState rasterizer = {};
        std::optional<RHIDepthStencilState> depthStencil = std::nullopt;
        RHIBlendState blend = {};

        RHIPixelFormat renderTargetFormats[RHI_MAX_RENDER_TARGETS] = {RHIPixelFormat::RGBA8_UNORM};
        uint32_t renderTargetCount = 1;
    };

    struct RHISize3D{
        uint32_t x, y, z;
    };

    struct RHIComputePipelineStateDesc{
        RHIShader* computeShader;
        RHISize3D gridSize;
        std::optional<RHISize3D> threadGroupSize = std::nullopt;
    };

    enum class RHIFilter: uint8_t{
        Nearest,
        Linear
    };

    enum class RHIAddressMode: uint8_t{
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
        float mipLODBias = 0.0f;
        float minLOD = 0.0f, maxLOD = std::numeric_limits<float>::max();

        uint32_t maxAnisotropy = 1;
        RHIComparisonFunc compareFunc = RHIComparisonFunc::Never;
        float borderColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    };

    constexpr auto LINEAR_WRAP_SAMPLER = RHISamplerState{};
    constexpr auto NEAREST_WRAP_SAMPLER = RHISamplerState{
        .minFilter = RHIFilter::Nearest,
        .magFilter = RHIFilter::Nearest,
        .mipFilter = RHIFilter::Nearest
    };

    constexpr auto RHI_FRAMES_IN_FLIGHT = 3;

    struct RHISwapchainCreateDesc{
        void* windowHandle; // Platform-specific window handle
        RHITextureCreateDesc bufferDesc;
        uint32_t bufferCount = RHI_FRAMES_IN_FLIGHT; // Triple buffering
        bool vsync = true;                           // VSync enabled by default
        bool allowTearing = false;                   // Variable refresh rate
    #if defined(_DEBUG) || !defined(NDEBUG)
        std::string debugName;
    #endif
    };

    inline size_t getBytesPerPixel(RHIPixelFormat format){
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

    enum class RHIBindingAccess: uint8_t{
        ReadOnly,
        ReadWrite,
        WriteOnly
    };

    struct RHIBufferViewDesc{
        RHIBindingAccess access;
        uint32_t offset, size;
        uint32_t stride = 0; // For structured buffers
    };

    struct RHITextureViewDesc{
        RHIBindingAccess access;

    };

    struct RHISlotBindingInfo{
        uint32_t index;
        RHIBindingAccess access;
    };

    struct RHIShaderBindingInfo{
        std::unordered_map<std::string, RHISlotBindingInfo> bufferInfo;
        std::unordered_map<std::string, RHISlotBindingInfo> textureInfo;
        std::unordered_map<std::string, RHISlotBindingInfo> samplerInfo;
    };

    struct RHIGraphicsBindingInfo{
        RHIShaderBindingInfo vsInfo;
        RHIShaderBindingInfo fsInfo;
    };

    struct RHIComputeBindingInfo{
        RHIShaderBindingInfo csInfo;
    };
}
