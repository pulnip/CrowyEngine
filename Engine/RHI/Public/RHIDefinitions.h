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

// Buffer creation descriptor
typedef struct{
    size_t size;
    RHIBufferUsageFlags usage;
    uint32_t stride; // For structured buffers
    const void* initialData;
    const char* debugName;
} RHIBufferCreateDesc;

#ifdef __cplusplus
}
#endif
