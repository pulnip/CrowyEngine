#include "RenderDefinitions.hpp"

namespace Crowy
{
    #ifdef CROWY_METALRHI
    size_t stride_of(CBufferFieldType type){
        switch(type){
        case CBufferFieldType::Int32:    return 4;
        case CBufferFieldType::Float:    return 4;
        case CBufferFieldType::Float2:   return 8;
        case CBufferFieldType::Float3:   return 16;
        case CBufferFieldType::Float4:   return 16;
        case CBufferFieldType::Float4x4: return 64;
        default:
            std::unreachable();
        }
    }
    #elifdef CROWY_D3DRHI
    size_t stride_of(CBufferFieldType type){
        return 16;
    }
    #endif
}