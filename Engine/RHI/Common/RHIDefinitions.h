#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef struct{
    bool flipTextureV;
    float clipSpaceMinZ;
} RHICapabilities;

typedef enum{
    BUF_None            = 0,
    BUF_VertexBuffer     = 1 << 0,
    BUF_IndexBuffer      = 1 << 1,
    BUF_ConstantBuffer   = 1 << 2,
    BUF_StructuredBuffer = 1 << 3,
    BUF_ShaderResource   = 1 << 4,
    BUF_UnorderedAccess  = 1 << 5,
    BUF_IndirectArgs     = 1 << 6,
    BUF_CopySource       = 1 << 7,
    BUF_CopyDest         = 1 << 8,
    BUF_CPUWrite         = 1 << 9,
    BUF_TransferSrc      = 1 << 10
} RHIBufferUsage;

typedef struct{
    size_t size;
    RHIBufferUsage usage;
    uint32_t stride; // For structured buffers
    const void* initialData;
    const char* debugName;
} RHIBufferCreateDesc;

typedef enum{
    PointList     = 0,
    LineList      = 1,
    LineStrip     = 2,
    TriangleList  = 3,
    TriangleStrip = 4,
} RHIPrimitiveTopology;

typedef enum{
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
} RHITextureFormat;

typedef enum{
    TEX_None            = 0,
    TEX_ShaderResource  = 1 << 0,
    TEX_RenderTarget    = 1 << 1,
    TEX_DepthStencil    = 1 << 2,
    TEX_UnorderedAccess = 1 << 3,
    TEX_CopySource      = 1 << 4,
    TEX_CopyDest        = 1 << 5,
} RHITextureUsage;

typedef enum{
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
} RHIResourceState;

typedef struct{
    float r, g, b, a;
} RHIClearColor;

typedef struct{
    float depth;
    uint8_t stencil;
} RHIClearDepthStencil;

typedef struct{
    uint32_t width;
    uint32_t height;
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
} RHITextureCreateDesc;

typedef enum{
    VertexShader   = 0,
    FragmentShader = 1,
    ComputeShader  = 2,
} RHIShaderStage;

typedef struct{
    const char* file; // source or binary file path
    const char* entry;
    RHIShaderStage stage;
    const char* debugName;
} RHIShaderCreateDesc;

typedef enum{
    Load,     // Preserve existing contents
    Store,    // Save contents
    Clear,    // Clear to specified color
    DontCare, // Don't care about existing contents
} RHILoadStoreAction;

typedef enum{
    IDX_UInt16,
    IDX_UInt32,
} RHIIndexFormat;

typedef struct{
    float x, y;
    float width, height;
    float minDepth, maxDepth;
} RHIViewport;

typedef struct{
    int32_t left, top;
    int32_t right, bottom;
} RHIScissorRect;

typedef enum{
    PerVertex,
    PerInstance,
} RHIInputClassification;

typedef struct{
    const char* semanticName;
    uint32_t semanticIndex;
    RHITextureFormat format;
    uint32_t inputSlot;
    uint32_t alignedByteOffset;
    RHIInputClassification classification;
    uint32_t instanceDataStepRate; // For per-instance data
} RHIVertexElement;

typedef struct{
    const RHIVertexElement* elements;
    uint32_t elementCount;
} RHIVertexLayout;

typedef enum{
    CullNone,
    Front,
    Back,
} RHICullMode;

typedef enum{
    Solid,
    Wireframe,
} RHIFillMode;

typedef struct{
    RHIFillMode fillMode;
    RHICullMode cullMode;
    bool frontCounterClockwise;
    int32_t depthBias;
    float depthBiasClamp;
    float slopeScaledDepthBias;
    bool depthClipEnable;
    bool multisampleEnable;
    bool antialiasedLineEnable;
} RHIRasterizerState;

typedef enum{
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always,
} RHIComparisonFunc;

typedef struct{
    bool depthEnable;
    bool depthWriteEnable;
    RHIComparisonFunc depthFunc;
    bool stencilEnable;
    uint8_t stencilReadMask;
    uint8_t stencilWriteMask;
} RHIDepthStencilState;

typedef enum{
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
} RHIBlend;

typedef enum{
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
} RHIBlendOp;

typedef struct{
    bool blendEnable;
    RHIBlend srcBlend;
    RHIBlend dstBlend;
    RHIBlendOp blendOp;
    RHIBlend srcBlendAlpha;
    RHIBlend dstBlendAlpha;
    RHIBlendOp blendOpAlpha;
    uint8_t renderTargetWriteMask; // RGBA
} RHIRenderTargetBlendState;

typedef struct{
    bool alphaToCoverageEnable;
    bool independentBlendEnable;
    RHIRenderTargetBlendState renderTargets[8];
} RHIBlendState;

constexpr auto RHI_MAX_RENDER_TARGETS = 8;

typedef struct{
    uint64_t vertexShaderIndex;
    uint64_t pixelShaderIndex;

    RHIVertexLayout vertexLayout;
    RHIPrimitiveTopology topology;

    RHIRasterizerState rasterizer;
    RHIDepthStencilState depthStencil;
    RHIBlendState blend;

    RHITextureFormat renderTargetFormats[RHI_MAX_RENDER_TARGETS];
    uint32_t renderTargetCount;
    RHITextureFormat depthStencilFormat;
    const char* debugName;
} RHIGraphicsPipelineStateDesc;

typedef struct{
    uint64_t computeShader;
    const char* debugName;
} RHIComputePipelineStateDesc;

typedef struct{
    void* windowHandle;   // Platform-specific window handle
    uint32_t width;
    uint32_t height;
    RHITextureFormat format;
    uint32_t bufferCount; // Triple buffering
    bool vsync;           // VSync enabled by default
    bool allowTearing;    // Variable refresh rate
    const char* debugName;
} RHISwapchainCreateDesc;

#ifdef __cplusplus
}
#endif