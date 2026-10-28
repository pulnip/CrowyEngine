#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <type_traits>
#include <vector>

#include "LinearAlgebra.hpp"
#include "Primitives.hpp"

// A particle effect is one .slang file and one ParticleEffectDesc; what the
// GPU reads is mirrored from Effect.slang.
namespace Crowy
{
    struct EffectDrawDesc;

    using EffectDrawDescs = std::vector<EffectDrawDesc>;

    // the draw ParticleKernel.slang's first step staggers ages with
    inline constexpr u32 ParticleStaggerDraw = 0xA6E;
    inline constexpr f32 EffectStep = 1.0f / 60.0f;

    enum class EffectBlend : u8 {
        // light added, order-free
        Additive,
        // over every effect drawn before it, near or far: list it first
        Alpha,
    };

    struct EffectDrawDesc {
        // vs_<entry> and fs_<entry> in the effect's file
        Str entry;
        EffectBlend blend = EffectBlend::Additive;
        // a strip per particle
        u32 verticesPerInstance = 4;
    };

    struct ParticleEffectDesc {
        Str name;
        // spawn, update and the draws' entries; it includes
        // ParticleKernel.slang last, which defines cs_step
        std::filesystem::path shader;
        u32 count = 0;
        u32 seed = 0;
        // steps the first frame runs before it shows, so an effect starts
        // in its steady state
        u32 prewarmSteps = 0;
        // where the effect sits and how far it reaches around there; what
        // moves an effect moves only its origin
        Vec3 origin{};
        f32 size = 0.0f;
        // the effect's own lanes, named by the accessors atop its file
        std::array<Vec4, 4> params{};
        EffectDrawDescs draws;
    };

    // the camera's basis the billboards face
    struct EffectView {
        Vec3 right = unitX();
        Vec3 up = unitY();
    };

    struct EffectParticle {
        Vec3 position{};
        u32 ageSteps = 0;
        Vec3 velocity{};
        u32 lifeSteps = 0;
        Vec4 custom{};
        u32 generation = 0;
        f32 size = 0.0f;
        u32 _pad0 = 0;
        u32 _pad1 = 0;
    };
    static_assert(sizeof(EffectParticle) == 64);
    static_assert(offsetof(EffectParticle, ageSteps) == 12);
    static_assert(offsetof(EffectParticle, lifeSteps) == 28);
    static_assert(offsetof(EffectParticle, custom) == 32);
    static_assert(offsetof(EffectParticle, generation) == 48);
    static_assert(std::is_trivially_copyable_v<EffectParticle>);

    // scalars and 16-byte vectors only, so HLSL and Metal lay it out alike
    struct EffectPush {
        u64 particlesRW = 0;
        u64 particles = 0;
        u32 count = 0;
        u32 step = 0;
        f32 dt = 0.0f;
        u32 seed = 0;
        Vec4 cameraRight{};
        Vec4 cameraUp{};
        // the desc's origin; w is unused
        Vec4 origin{};
        std::array<Vec4, 4> params{};
        // the world's step this dispatch or draw shows, shared with the scene
        u32 worldStep = 0;
        f32 size = 0.0f;
        u32 _pad0 = 0;
        u32 _pad1 = 0;
    };
    static_assert(sizeof(EffectPush) == 160);
    static_assert(offsetof(EffectPush, cameraRight) == 32);
    static_assert(offsetof(EffectPush, origin) == 64);
    static_assert(offsetof(EffectPush, params) == 80);
    static_assert(offsetof(EffectPush, worldStep) == 144);
    static_assert(offsetof(EffectPush, size) == 148);

    // the right and up a world-to-view matrix rotates onto x and y
    inline constexpr EffectView effectViewOf(const Mat4& view) {
        return EffectView{
            .right = Vec3{view[0].x, view[1].x, view[2].x},
            .up = Vec3{view[0].y, view[1].y, view[2].y}
        };
    }
}
