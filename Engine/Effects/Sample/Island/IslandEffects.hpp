#pragma once

#include "IslandScene.h"
#include "LinearAlgebra.hpp"
#include "ParticleEffects.hpp"

// The Island's effects as descs, shared with the check that replays its rain.
namespace Crowy
{
    // a shower over the fire and the shore, its box upwind of them by the
    // drift of a fall, landing on the island's shapes
    inline ParticleEffectDesc rainDesc() {
        return ParticleEffectDesc{
            .name = "rain",
            .shader = "Engine/Effects/Sample/Island/Rain.slang",
            .count = 4200,
            .seed = 23,
            .prewarmSteps = 120,
            .emitter = Vec4{-1.42f, 6.5f, 1.53f, 6.0f},
            .params =
                {Vec4{9.5f, 9.5f, 9.0f, 0.0f},
                 Vec4{0.0f, 0.03f, 0.35f, 0.28f},
                 Vec4{}},
            .draws =
                {{.entry = "ripples", .blend = EffectBlend::Alpha},
                 {.entry = "streaks", .blend = EffectBlend::Additive}}
        };
    }

    // a dome of stars that follows the camera, a few hundred in frame
    inline ParticleEffectDesc starsDesc() {
        return ParticleEffectDesc{
            .name = "stars",
            .shader = "Engine/Effects/Sample/Island/Stars.slang",
            .count = 2560,
            .seed = 53,
            .emitter = Vec4{0.0f, 0.0f, 0.0f, 170.0f},
            .params =
                {Vec4{0.0349f, 1.5f, 8.0f, 0.0f},
                 Vec4{0.1f, 2.2f, 7.0f, 0.0f},
                 Vec4{}},
            .draws = {{.entry = "stars", .blend = EffectBlend::Additive}}
        };
    }

    // a shower from a radiant low on the left, about one in the sky at a
    // time; the seed puts one high on the left at frame 60
    inline ParticleEffectDesc meteorsDesc() {
        return ParticleEffectDesc{
            .name = "meteors",
            .shader = "Engine/Effects/Sample/Island/Meteors.slang",
            .count = 12,
            .seed = 1,
            .emitter = Vec4{-1.6581f, 0.0873f, 160.0f, 0.0f},
            .params =
                {Vec4{-0.7854f, 0.6109f, 0.1745f, 0.4189f},
                 Vec4{0.262f, 0.524f, 0.25f, 2.0f},
                 Vec4{1.2f, 0.125f, 0.0f, 0.0f}},
            .draws = {{.entry = "meteors", .blend = EffectBlend::Additive}}
        };
    }

    // tongues off the coals, up to where the tripod's logs cross
    inline ParticleEffectDesc flamesDesc() {
        constexpr Vec3 Fire{ISLAND_FIRE};
        constexpr auto Crossing = ISLAND_LOG_CROSSING_Y - ISLAND_COALS_TOP_Y;

        return ParticleEffectDesc{
            .name = "flames",
            .shader = "Engine/Effects/Sample/Island/Flames.slang",
            .count = 90,
            .seed = 13,
            .prewarmSteps = 60,
            .emitter = toVec4(
                Vec3{Fire.x, ISLAND_COALS_TOP_Y, Fire.z},
                ISLAND_COALS_HALF
            ),
            .params =
                {Vec4{0.6f, 1.1f, 2.6f, 2.0f},
                 Vec4{Crossing, 2.5f, 0.08f, 0.13f},
                 Vec4{1.0f, 2.0f, 24.0f, 30.0f}},
            .draws = {{.entry = "flames", .blend = EffectBlend::Additive}}
        };
    }

    // sparks off the flames' top, up the chimney and out of the smoke hole,
    // two to five seconds each; the prewarm outlasts the longest
    inline ParticleEffectDesc embersDesc() {
        return ParticleEffectDesc{
            .name = "embers",
            .shader = "Engine/Effects/Sample/Island/Embers.slang",
            .count = 384,
            .seed = 11,
            .prewarmSteps = 320,
            .emitter =
                toVec4(Vec3{ISLAND_FIRE} + Vec3{0.0f, 0.45f, 0.0f}, 0.1f),
            .params =
                {Vec4{0.6f, 1.2f, 2.4f, 1.5f},
                 Vec4{0.8f, 0.025f, 2.5f, 0.3f},
                 Vec4{2.0f, 1.5f, 150.0f, 150.0f}},
            .draws = {{.entry = "embers", .blend = EffectBlend::Additive}}
        };
    }
}
