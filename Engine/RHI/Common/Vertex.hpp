#pragma once

#include <array>
#include <type_traits>
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    // Standard vertex format
    // Matches common 3D model formats (glTF, FBX, OBJ)
    struct Vertex{
        Vec3 position;
        Vec3 normal;
        Vec2 texCoord;
        // xyz = tangent direction, along increasing u.
        // w = handedness sign, so the direction of increasing v is
        // w * cross(normal, tangent.xyz).
        Vec4 tangent;

        // Optional: vertex colors, bone weights, etc. can be added later
    };
    static_assert(sizeof(Vertex) == 48, "Vertex should be 48 bytes");
    static_assert(std::is_trivially_copyable_v<Vertex>, "Vertex must be trivially copyable");

    inline constexpr std::array VERTEX_INPUT_LAYOUT = {
        RHIVertexElement{
            .semanticName = "POSITION",
            .semanticIndex = 0,
            .format = RHIPixelFormat::RGB32_FLOAT,  // float3 (12 bytes)
            .inputSlot = 0,
            .alignedByteOffset = 0,
            .classification = RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        },
        RHIVertexElement{
            .semanticName = "NORMAL",
            .semanticIndex = 0,
            .format = RHIPixelFormat::RGB32_FLOAT,  // float3 (12 bytes)
            .inputSlot = 0,
            .alignedByteOffset = 12,
            .classification = RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        },
        RHIVertexElement{
            .semanticName = "TEXCOORD",
            .semanticIndex = 0,
            .format = RHIPixelFormat::RG32_FLOAT,
            .inputSlot = 0,
            .alignedByteOffset = 24,
            .classification = RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        },
        RHIVertexElement{
            .semanticName = "TANGENT",
            .semanticIndex = 0,
            .format = RHIPixelFormat::RGBA32_FLOAT,
            .inputSlot = 0,
            .alignedByteOffset = 32,
            .classification = RHIInputClassification::PerVertex,
            .instanceDataStepRate = 0
        }
    };
}
