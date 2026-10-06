#pragma once

#include "MintFrame.hpp"
#include "PaintShared.h"
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

    // the size of the paint-id space: 0-3 teams, 4-6 reserved, the last
    // meaning "nothing painted here", so painting it erases
    inline constexpr u8 PaintIdCount = 8;
    inline constexpr u8 PaintIdNone = PaintIdCount - 1;
    static_assert(PaintIdNone == PAINT_ID_NONE);
    inline constexpr u8 PaintTeamIdCount = 4;
    // a texel's R byte: the id in the low three bits, a star generation above
    inline constexpr u8 PaintIdBits = 3;
    inline constexpr u8 PaintStarGenMax = 31;

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

    // which generation each team has locked: a texel of that team carrying
    // exactly that generation cannot be painted over by any other id
    struct PaintLockGens {
        u8 gen[PaintTeamIdCount]{};

        constexpr u8 For(u8 paintId) const noexcept {
            return paintId < PaintTeamIdCount ? gen[paintId] : 0;
        }
        constexpr bool Locks(u8 paintId, u8 starGen) const noexcept {
            return starGen != 0 && starGen == For(paintId);
        }
    };

    struct PaintSplashProfile;

    // FPaintSplat: one contact, fully resolved; the server builds it once and
    // every machine draws the identical stamp from it
    struct PaintSplat {
        // stamp centre in world space, the incidence shift applied
        DVec3 location;
        DVec3 normal{0.0, 0.0, 1.0};
        // unit stamp U axis: the stretch direction, or a seeded rotation
        DVec3 axisU{1.0, 0.0, 0.0};
        // half-extent along V in cm; along U the stamp spans radius * stretch
        f32 radius = 25.0f;
        // 1 / cos(incidence), clamped; 1 is a head-on hit
        f32 stretch = 1.0f;
        // the contact along U, normalized by the long half-axis
        f32 impactU = 0.0f;
        u8 paintId = 0;
        u8 starGen = 0;
        PaintLockGens lockGens;
        // the fraction of the max paint height this contact adds
        f32 heightAdd = 0.35f;
        // 16 bits, all the stamp shader's sin hash keeps
        u16 seed = 0;
        // BrushShapeNoise of the brush material that stamps it
        f32 shapeNoise = 1.0f;
        // a contact on a direction the surface does not keep: an effect only
        bool transient = false;
        // what the contact scatters beyond this splat, or null
        const PaintSplashProfile* splash = nullptr;
        DVec3 incidentDir{1.0, 0.0, 0.0};
        // cm/s
        u16 incidentSpeed = 0;
        // cm
        u8 ballRadius = 0;
        // marks the score and draws nothing; made where a splash expands
        bool scoreOnly = false;
        // draws and marks no cell; a droplet's mark on the machine it landed
        bool drawOnly = false;

        f64 WorldExtent() const noexcept { return radius * stretch; }
    };

    // the splat in the mesh's scaled-local frame, so every length is still a
    // world one; the brush and the cell grid both read it
    struct PaintLocalStamp {
        DVec3 center;
        // unit axes of the stamp plane; U is the stretched one
        DVec3 axisU{1.0, 0.0, 0.0};
        DVec3 axisV{0.0, 1.0, 0.0};
        DVec3 normal{0.0, 0.0, 1.0};
        f32 radius = 0.0f;
        f32 stretch = 1.0f;
    };
}
