#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

// Opaque handle types for Swift objects
typedef void* MetalDevicePtr;
typedef void* MetalCommandListPtr;

typedef uint64_t MetalBufferID;
typedef uint64_t MetalTextureID;
typedef uint64_t MetalShaderID;
typedef uint64_t MetalPipelineStateID;
typedef uint64_t MetalSwapchainID;
typedef uint64_t MetalFenceID;

#ifdef __cplusplus
}
#endif
