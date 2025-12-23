#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

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
} RHIBufferUsageFlags;

typedef struct{
    size_t size;
    RHIBufferUsageFlags usage;
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

#ifdef __cplusplus
}

namespace Crowy
{
    struct RHICapabilities{
        bool flipTextureV;
        float clipSpaceMinZ;
    };
}
#endif