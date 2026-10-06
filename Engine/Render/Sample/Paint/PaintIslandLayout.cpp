#include "PaintIslandLayout.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <format>

namespace Crowy
{
    namespace
    {
        struct PendingIsland {
            PaintIsland island;
            i32 width = 0;
            i32 height = 0;
        };

        i32 roundUpToPowerOfTwo(i32 value) {
            return static_cast<i32>(
                std::bit_ceil(static_cast<u32>(std::max(value, 1)))
            );
        }

        // rows of islands, tallest first; exact enough for six rectangles
        bool tryPack(std::vector<PendingIsland>& pending, i32 atlasSize) {
            std::ranges::stable_sort(
                pending,
                [](const PendingIsland& a, const PendingIsland& b) {
                    return a.height > b.height;
                }
            );

            i32 x = 0;
            i32 y = 0;
            i32 shelfHeight = 0;
            for(auto& entry: pending) {
                if(x + entry.width > atlasSize) {
                    y += shelfHeight;
                    x = 0;
                    shelfHeight = 0;
                }
                if(y + entry.height > atlasSize || entry.width > atlasSize)
                    return false;
                entry.island.rect =
                    IntRect{{x, y}, {x + entry.width, y + entry.height}};
                x += entry.width;
                shelfHeight = std::max(shelfHeight, entry.height);
            }

            return true;
        }
    }

    void PaintIsland::PlaneAxes(i32 axis, i32& axisB, i32& axisC) noexcept {
        switch(axis) {
        case 0:
            axisB = 1;
            axisC = 2;
            break;
        case 1:
            axisB = 0;
            axisC = 2;
            break;
        default:
            axisB = 0;
            axisC = 1;
            break;
        }
    }

    Vec4 PaintIsland::ToShaderParam(i32 atlasSize) const noexcept {
        const auto inv = 1.0f / static_cast<f32>(std::max(atlasSize, 1));

        return {
            static_cast<f32>(contentOrigin.x) * inv,
            static_cast<f32>(contentOrigin.y) * inv,
            static_cast<f32>(contentTexels.x) * inv,
            static_cast<f32>(contentTexels.y) * inv
        };
    }

    PaintIslandLayout PaintIslandLayout::Build(
        const Box3d& localBounds,
        DVec3 scale3D,
        u8 enabledDirections,
        f32 requestedTexelCm,
        i32 padTexels,
        i32 minSize,
        i32 maxSize
    ) {
        PaintIslandLayout layout;
        layout.enabledDirections = enabledDirections;
        layout.texelCm = std::max(requestedTexelCm, 0.01f);
        if(enabledDirections == 0)
            return layout;

        minSize = roundUpToPowerOfTwo(minSize);
        maxSize = std::max(roundUpToPowerOfTwo(maxSize), minSize);
        // a gutter wider than the atlas can never fit, whatever the texel does
        padTexels = std::clamp(padTexels, 0, maxSize / 8);

        const auto worldSize = localBounds.Size() * absolute(scale3D);

        constexpr i32 MaxAttempts = 64;
        for(i32 attempt = 0; attempt < MaxAttempts; ++attempt) {
            std::vector<PendingIsland> pending;
            i64 totalArea = 0;
            i32 maxSide = 0;
            for(u8 d = 0; d < PaintFaceDirectionCount; ++d) {
                const auto face = paintFaceDirectionAt(d);
                if(!(enabledDirections & paintDirectionBit(face)))
                    continue;

                auto& entry = pending.emplace_back();
                auto& island = entry.island;
                island.direction = face;
                island.axis = d / 2;
                island.sign = d % 2 == 0 ? 1 : -1;
                PaintIsland::PlaneAxes(island.axis, island.axisB, island.axisC);
                island.contentTexels = {
                    worldSize[island.axisB] / layout.texelCm,
                    worldSize[island.axisC] / layout.texelCm
                };
                entry.width =
                    std::max(
                        1,
                        static_cast<i32>(std::ceil(island.contentTexels.x))
                    ) +
                    2 * padTexels;
                entry.height =
                    std::max(
                        1,
                        static_cast<i32>(std::ceil(island.contentTexels.y))
                    ) +
                    2 * padTexels;
                totalArea += static_cast<i64>(entry.width) * entry.height;
                maxSide = std::max({maxSide, entry.width, entry.height});
            }

            const auto needed = std::max(
                maxSide,
                static_cast<i32>(
                    std::ceil(std::sqrt(static_cast<f64>(totalArea)))
                )
            );
            auto atlasSize =
                std::clamp(roundUpToPowerOfTwo(needed), minSize, maxSize);
            for(; atlasSize <= maxSize; atlasSize *= 2) {
                if(!tryPack(pending, atlasSize))
                    continue;

                layout.atlasSize = atlasSize;
                layout.islands.clear();
                for(auto& entry: pending) {
                    entry.island.contentOrigin = {
                        entry.island.rect.min.x + padTexels,
                        entry.island.rect.min.y + padTexels
                    };
                    layout.islands.push_back(entry.island);
                }
                std::ranges::sort(
                    layout.islands,
                    [](const PaintIsland& a, const PaintIsland& b) {
                        return a.direction < b.direction;
                    }
                );

                return layout;
            }

            // nothing fits even at maxSize: coarsen toward the area that
            // would, and a bit more so the packing waste has room
            const auto fill = static_cast<f64>(totalArea) /
                              (0.85 * static_cast<f64>(maxSize) * maxSize);
            layout.texelCm = static_cast<f32>(
                layout.texelCm * std::max(1.05, std::sqrt(fill))
            );
        }

        layout.islands.clear();
        layout.atlasSize = 0;

        return layout;
    }

    const PaintIsland* PaintIslandLayout::Find(
        PaintFaceDirection direction
    ) const noexcept {
        const auto it =
            std::ranges::find(islands, direction, &PaintIsland::direction);

        return it == islands.end() ? nullptr : &*it;
    }

    Str PaintIslandLayout::ToString() const {
        auto result = std::format(
            "mask {:02x}, {} texels, {:.3f} cm",
            enabledDirections,
            atlasSize,
            texelCm
        );
        for(const auto& island: islands) {
            result += std::format(
                " | {}:[{},{} {}x{}] {:.2f}x{:.2f}",
                static_cast<i32>(island.direction),
                island.rect.min.x,
                island.rect.min.y,
                island.rect.Width(),
                island.rect.Height(),
                island.contentTexels.x,
                island.contentTexels.y
            );
        }

        return result;
    }
}
