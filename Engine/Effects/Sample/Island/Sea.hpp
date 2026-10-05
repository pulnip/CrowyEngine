#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "IslandShapes.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"
#include "SeaWaves.h"
#include "WorldClock.hpp"

// The CPU twin of Sea.slang, equal to the GPU's within a check's tolerance:
// the sea's height by place and world step.
namespace Crowy
{
    using SeaLane = std::array<f32, SEA_WAVE_COUNT>;
    using SeaCycleLane = std::array<u32, SEA_WAVE_COUNT>;

    inline constexpr SeaLane SeaInvLength{SEA_INV_LENGTHS};
    inline constexpr SeaLane SeaAmplitude{SEA_AMPLITUDES};
    inline constexpr SeaLane SeaDirectionX{SEA_DIRECTIONS_X};
    inline constexpr SeaLane SeaDirectionZ{SEA_DIRECTIONS_Z};
    inline constexpr SeaCycleLane SeaCycles{SEA_CYCLES};
    inline constexpr SeaLane SeaPhase{SEA_PHASES};

    // how much of a wave the polar grid can carry at radius r
    inline constexpr f32 seaCellFade(f32 r, f32 invLength) {
        constexpr auto TwoPi = 2.0f * std::numbers::pi_v<f32>;

        const auto cell = std::max(r, 3.5f) * TwoPi / 256.0f;
        return std::clamp(1.0f / (invLength * 4.0f * cell) - 1.0f, 0.0f, 1.0f);
    }

    // the waves damp toward the waterline, where the still water is shallow
    inline f32 seaShore(Vec2 xz) {
        const auto t = std::clamp(-islandCrown(xz) / 1.2f, 0.0f, 1.0f);
        const auto eased = t * t * (3.0f - 2.0f * t);

        return 0.2f + 0.8f * eased;
    }

    // the first `count` waves at xz; `onGrid` fades those the grid cannot
    // carry
    inline f32 seaHeight(Vec2 xz, u32 step, u32 count, bool onGrid) {
        constexpr auto Pi = std::numbers::pi_v<f32>;

        const auto shore = seaShore(xz);
        const auto r = std::sqrt(dot(xz, xz));
        auto p = xz;
        auto height = 0.0f;
        for(u32 i = 0; i < count; ++i) {
            auto amplitude = SeaAmplitude[i] * shore;
            if(onGrid)
                amplitude *= seaCellFade(r, SeaInvLength[i]);
            const Vec2 direction{SeaDirectionX[i], SeaDirectionZ[i]};
            auto c = dot(direction, p) * SeaInvLength[i] -
                     loopPhase(step, SeaCycles[i]) + SeaPhase[i];
            c -= std::floor(c);
            const auto angle = 2.0f * Pi * c - Pi;
            const auto crest = std::exp(std::sin(angle) - 1.0f);
            height += amplitude * (crest - SEA_MEAN);
            const auto drag = SEA_DRAG * amplitude / SeaAmplitude[0] /
                              (SeaInvLength[0] * 2.0f * Pi);
            p = p - direction * (crest * std::cos(angle) * drag);
        }

        return height;
    }
}
