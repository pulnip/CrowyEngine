#pragma once

#include "MintFrame.hpp"
#include "Primitives.hpp"

// MintChoco's PaintCellGrid.h directions and PaintSplat.h texel encoding
namespace Crowy
{
    // the six ways a piece of surface faces in the painted mesh's local frame
    enum class PaintFaceDirection : u8 {
        Front, // +X
        Back,  // -X
        Right, // +Y
        Left,  // -Y
        Up,    // +Z
        Down,  // -Z
    };

    inline constexpr u8 PaintFaceDirectionCount = 6;
    // one bit per PaintFaceDirection, in enum order
    inline constexpr u8 PaintAllDirectionsMask = 0x3F;

    inline constexpr u8 paintDirectionBit(PaintFaceDirection direction) {
        return static_cast<u8>(1u << static_cast<u8>(direction));
    }

    inline constexpr PaintFaceDirection paintFaceDirectionAt(u8 index) {
        return static_cast<PaintFaceDirection>(index);
    }

    // the unit vector, in mesh local space, the direction faces
    inline constexpr DVec3 paintFaceDirectionVector(PaintFaceDirection d) {
        using enum PaintFaceDirection;

        switch(d) {
        case Front:
            return {1.0, 0.0, 0.0};
        case Back:
            return {-1.0, 0.0, 0.0};
        case Right:
            return {0.0, 1.0, 0.0};
        case Left:
            return {0.0, -1.0, 0.0};
        case Up:
            return {0.0, 0.0, 1.0};
        case Down:
            return {0.0, 0.0, -1.0};
        }

        return {0.0, 0.0, 1.0};
    }

    // the direction whose axis the normal leans on most; ties resolve X over
    // Y over Z
    inline constexpr PaintFaceDirection classifyPaintFaceDirection(DVec3 n) {
        using enum PaintFaceDirection;

        const auto a = absolute(n);
        if(a.x >= a.y && a.x >= a.z)
            return n.x >= 0.0 ? Front : Back;
        if(a.y >= a.z)
            return n.y >= 0.0 ? Right : Left;

        return n.z >= 0.0 ? Up : Down;
    }

    // the size of the paint-id space: 0-3 teams, 4-6 reserved, the last
    // meaning "nothing painted here", so painting it erases
    inline constexpr u8 PaintIdCount = 8;
    inline constexpr u8 PaintIdNone = PaintIdCount - 1;
    inline constexpr u8 PaintTeamIdCount = 4;
    // a texel's R byte: the id in the low three bits, a star generation above
    inline constexpr u8 PaintIdBits = 3;
    inline constexpr u8 PaintStarGenMax = 31;

    inline constexpr u8 encodePaintTexel(u8 paintId, u8 starGen) {
        // unpainted carries no generation, so the clear value stays bare
        const u8 gen =
            paintId == PaintIdNone
                ? 0
                : (starGen > PaintStarGenMax ? PaintStarGenMax : starGen);
        return static_cast<u8>(paintId | (gen << PaintIdBits));
    }

    inline constexpr u8 decodePaintId(u8 texel) {
        return static_cast<u8>(texel & (PaintIdCount - 1));
    }

    inline constexpr u8 decodePaintStarGen(u8 texel) {
        return static_cast<u8>(texel >> PaintIdBits);
    }
}
