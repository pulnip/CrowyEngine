#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include "enum_traits.hpp"
#include "math.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    struct RHICapabilities{
        bool flipTextureV;
        float clipSpaceMinZ;
    };

    enum class RHIBufferUsage: uint16_t{
        None             = 0,
        VertexBuffer     = 1 << 0,
        IndexBuffer      = 1 << 1,
        ConstantBuffer   = 1 << 2,
        StructuredBuffer = 1 << 3,
        ShaderResource   = 1 << 4,
        UnorderedAccess  = 1 << 5,
        IndirectArgs     = 1 << 6,
        CopySource       = 1 << 7,
        CopyDest         = 1 << 8,
        CPUWrite         = 1 << 9,
        TransferSrc      = 1 << 10
    };

    struct RHIBufferCreateDesc{
        size_t size;
        RHIBufferUsage usage;
        uint32_t stride; // For structured buffers
        const void* initialData;
        const char* debugName;
    };

    enum class RHIPrimitiveTopology{
        PointList     = 0,
        LineList      = 1,
        LineStrip     = 2,
        TriangleList  = 3,
        TriangleStrip = 4,
    };

    enum class RHITextureFormat{
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
        None            = 0,
        ShaderResource  = 1 << 0,
        RenderTarget    = 1 << 1,
        DepthStencil    = 1 << 2,
        UnorderedAccess = 1 << 3,
        CopySource      = 1 << 4,
        CopyDest        = 1 << 5,
    };

    enum class RHIResourceState{
        Common,
        VertexBuffer,
        IndexBuffer,
        ConstantBuffer,
        ShaderResource,
        UnorderedAccess,
        RenderTarget,
        DepthStencilWrite,
        DepthStencilRead,
        CopySource,
        CopyDest,
        Present,
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
        uint32_t width, height;
        uint32_t depth;
        uint32_t mipLevels;
        uint32_t arraySize;
        RHITextureFormat format;
        RHITextureUsage usage;
        RHIResourceState initialState;
        RHIClearColor clearColor;
        RHIClearDepthStencil clearDepthStencil;
        const void* initialData;
        const char* debugName;
    };

    enum class RHIShaderStage{
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
        const char* debugName;
    };

    enum class RHILoadStoreAction{
        Load,     // Preserve existing contents
        Store,    // Save contents
        Clear,    // Clear to specified color
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

    enum class RHIInputClassification{
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
        RHITextureFormat format;
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
            .format = RHITextureFormat::RGB32_FLOAT,  // float3 (12 bytes)
            .inputSlot = 0,
            .alignedByteOffset = 0,
            .classification = RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        },
        {
            .semanticName = "NORMAL",
            .semanticIndex = 0,
            .format = RHITextureFormat::RGB32_FLOAT,  // float3 (12 bytes)
            .inputSlot = 0,
            .alignedByteOffset = 12,
            .classification = RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        },
        {
            .semanticName = "TEXCOORD",
            .semanticIndex = 0,
            .format = RHITextureFormat::RG32_FLOAT,
            .inputSlot = 0,
            .alignedByteOffset = 24,
            .classification = RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        },
        {
            .semanticName = "TANGENT",
            .semanticIndex = 0,
            .format = RHITextureFormat::RGBA32_FLOAT,
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

    enum class RHICullMode{
        CullNone,
        Front,
        Back,
    };

    enum class RHIFillMode{
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

    enum class RHIComparisonFunc{
        Never,
        Less,
        Equal,
        LessEqual,
        Greater,
        NotEqual,
        GreaterEqual,
        Always,
    };

    struct RHIDepthStencilState{
        bool depthEnable = true;
        bool depthWriteEnable = true;
        RHIComparisonFunc depthFunc = RHIComparisonFunc::Less;
        bool stencilEnable = false;
        uint8_t stencilReadMask = 0xFF;
        uint8_t stencilWriteMask = 0xFF;
    };

    enum class RHIBlend{
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

    enum class RHIBlendOp{
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
        RHIDepthStencilState depthStencil = {};
        RHIBlendState blend = {};

        RHITextureFormat renderTargetFormats[RHI_MAX_RENDER_TARGETS] = {RHITextureFormat::RGBA8_UNORM};
        uint32_t renderTargetCount = 1;
        RHITextureFormat depthStencilFormat = RHITextureFormat::D32_FLOAT;
        const char* debugName = nullptr;
    };

    struct RHIComputePipelineStateDesc{
        RHIShader* computeShader;
        const char* debugName;
    };

    struct RHISwapchainCreateDesc{
        void* windowHandle;   // Platform-specific window handle
        uint32_t width;
        uint32_t height;
        RHITextureFormat format;
        uint32_t bufferCount; // Triple buffering
        bool vsync;           // VSync enabled by default
        bool allowTearing;    // Variable refresh rate
        const char* debugName;
    };

    inline size_t getBytesPerPixel(RHITextureFormat format){
        switch(format){
        case RHITextureFormat::R8_UNORM:          [[fallthrough]];
        case RHITextureFormat::R8_SNORM:          [[fallthrough]];
        case RHITextureFormat::R8_UINT:           [[fallthrough]];
        case RHITextureFormat::R8_SINT:
            return 1;
        case RHITextureFormat::R16_UNORM:         [[fallthrough]];
        case RHITextureFormat::R16_SNORM:         [[fallthrough]];
        case RHITextureFormat::R16_UINT:          [[fallthrough]];
        case RHITextureFormat::R16_SINT:          [[fallthrough]];
        case RHITextureFormat::R16_FLOAT:         [[fallthrough]];
        case RHITextureFormat::RG8_UNORM:         [[fallthrough]];
        case RHITextureFormat::RG8_SNORM:         [[fallthrough]];
        case RHITextureFormat::RG8_UINT:          [[fallthrough]];
        case RHITextureFormat::RG8_SINT:
            return 2;
        case RHITextureFormat::R32_UINT:          [[fallthrough]];
        case RHITextureFormat::R32_SINT:          [[fallthrough]];
        case RHITextureFormat::R32_FLOAT:         [[fallthrough]];
        case RHITextureFormat::RG16_UNORM:        [[fallthrough]];
        case RHITextureFormat::RG16_SNORM:        [[fallthrough]];
        case RHITextureFormat::RG16_UINT:         [[fallthrough]];
        case RHITextureFormat::RG16_SINT:         [[fallthrough]];
        case RHITextureFormat::RG16_FLOAT:        [[fallthrough]];
        case RHITextureFormat::RGBA8_UNORM:       [[fallthrough]];
        case RHITextureFormat::RGBA8_UNORM_SRGB:  [[fallthrough]];
        case RHITextureFormat::RGBA8_SNORM:       [[fallthrough]];
        case RHITextureFormat::RGBA8_UINT:        [[fallthrough]];
        case RHITextureFormat::RGBA8_SINT:        [[fallthrough]];
        case RHITextureFormat::BGRA8_UNORM:       [[fallthrough]];
        case RHITextureFormat::BGRA8_UNORM_SRGB:
            return 4;
        case RHITextureFormat::RG32_UINT:         [[fallthrough]];
        case RHITextureFormat::RG32_SINT:         [[fallthrough]];
        case RHITextureFormat::RG32_FLOAT:        [[fallthrough]];
        case RHITextureFormat::RGBA16_UNORM:      [[fallthrough]];
        case RHITextureFormat::RGBA16_SNORM:      [[fallthrough]];
        case RHITextureFormat::RGBA16_UINT:       [[fallthrough]];
        case RHITextureFormat::RGBA16_SINT:       [[fallthrough]];
        case RHITextureFormat::RGBA16_FLOAT:
            return 8;
        case RHITextureFormat::RGB32_FLOAT:
            return 12;
        case RHITextureFormat::RGBA32_UINT:       [[fallthrough]];
        case RHITextureFormat::RGBA32_SINT:       [[fallthrough]];
        case RHITextureFormat::RGBA32_FLOAT:
            return 16;
        case RHITextureFormat::D16_UNORM:
            return 2;
        case RHITextureFormat::D24_UNORM_S8_UINT: [[fallthrough]];
        case RHITextureFormat::D32_FLOAT:
            return 4;
        case RHITextureFormat::D32_FLOAT_S8_UINT:
            return 8;
        default:
            std::unreachable();
        }
    }
}
