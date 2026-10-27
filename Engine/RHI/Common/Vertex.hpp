#pragma once

#include <array>
#include <cstddef>
#include <type_traits>

#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    struct Vertex {
        Vec3 position;
        Vec3 normal;
        Vec2 texCoord;
        // xyz runs along increasing u; w is the handedness, so increasing v
        // runs along w * cross(normal, tangent.xyz)
        Vec4 tangent;
    };
    static_assert(sizeof(Vertex) == 48, "Vertex should be 48 bytes");
    static_assert(
        std::is_trivially_copyable_v<Vertex>,
        "Vertex must be trivially copyable"
    );

    inline constexpr std::array VertexInputLayout = {
        RHIVertexElement{
            .semanticName = "POSITION",
            .semanticIndex = 0,
            .format = RHIPixelFormat::RGB32_FLOAT,
            .inputSlot = 0,
            .alignedByteOffset = static_cast<u32>(offsetof(Vertex, position)),
            .classification = RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        },
        RHIVertexElement{
            .semanticName = "NORMAL",
            .semanticIndex = 0,
            .format = RHIPixelFormat::RGB32_FLOAT,
            .inputSlot = 0,
            .alignedByteOffset = static_cast<u32>(offsetof(Vertex, normal)),
            .classification = RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        },
        RHIVertexElement{
            .semanticName = "TEXCOORD",
            .semanticIndex = 0,
            .format = RHIPixelFormat::RG32_FLOAT,
            .inputSlot = 0,
            .alignedByteOffset = static_cast<u32>(offsetof(Vertex, texCoord)),
            .classification = RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        },
        RHIVertexElement{
            .semanticName = "TANGENT",
            .semanticIndex = 0,
            .format = RHIPixelFormat::RGBA32_FLOAT,
            .inputSlot = 0,
            .alignedByteOffset = static_cast<u32>(offsetof(Vertex, tangent)),
            .classification = RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        }
    };
}
