#pragma once

#include <cstdint>
#include <string>
#include "RenderDefinitions.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    struct CBuffer{
        using FieldName = CBufferFieldName;
        using FieldType = CBufferFieldType;
        using FieldOffset = CBufferFieldOffset;
        using FieldMeta = CBufferFieldMeta;

        std::string name;
        uint32_t slot;
        CBufferMeta meta;
        RHIBufferPtr buffer;
    };
}