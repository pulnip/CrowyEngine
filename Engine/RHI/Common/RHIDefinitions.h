#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef enum{
    BUF_None             = 0,
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