#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

#include "LinearAlgebra.hpp"
#include "Primitives.hpp"

// The slice of Unreal's math MintChoco's paint code runs on, in its frame:
// centimetres, X forward, Y right, Z up, left-handed, angles in degrees.
// Paint math stays in this frame; mintToCrowy() is the one way out.
namespace Crowy
{
    inline constexpr f64 MintCmToCrowyM = 0.01;
    // UE_SMALL_NUMBER and UE_KINDA_SMALL_NUMBER, in the types Unreal uses them
    inline constexpr f64 SmallNumber = 1e-8;
    inline constexpr f32 KindaSmallNumber = 1e-4f;
    inline constexpr f64 DoubleKindaSmallNumber = 1e-4;

    // FMath::RoundToInt
    inline i32 roundToInt(f32 x) noexcept {
        return static_cast<i32>(std::floor(x + 0.5f));
    }

    // FVector
    struct DVec3 {
        f64 x = 0.0;
        f64 y = 0.0;
        f64 z = 0.0;

        constexpr auto& operator[](this auto& self, usize i) noexcept {
            switch(i) {
            case 0:
                return self.x;
            case 1:
                return self.y;
            default:
                CROWY_ASSERT(i == 2, "DVec3 index out of range");
                return self.z;
            }
        }

        friend constexpr bool operator==(DVec3, DVec3) = default;
    };

    inline constexpr DVec3 operator+(DVec3 lhs, DVec3 rhs) noexcept {
        return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
    }

    inline constexpr DVec3 operator-(DVec3 lhs, DVec3 rhs) noexcept {
        return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
    }

    inline constexpr DVec3 operator-(DVec3 v) noexcept {
        return {-v.x, -v.y, -v.z};
    }

    inline constexpr DVec3 operator*(DVec3 v, f64 s) noexcept {
        return {v.x * s, v.y * s, v.z * s};
    }

    inline constexpr DVec3 operator*(f64 s, DVec3 v) noexcept {
        return v * s;
    }

    inline constexpr DVec3 operator*(DVec3 lhs, DVec3 rhs) noexcept {
        return {lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z};
    }

    inline constexpr DVec3 operator/(DVec3 v, f64 s) noexcept {
        return {v.x / s, v.y / s, v.z / s};
    }

    inline constexpr DVec3 operator/(DVec3 lhs, DVec3 rhs) noexcept {
        return {lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z};
    }

    inline constexpr f64 dot(DVec3 lhs, DVec3 rhs) noexcept {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
    }

    // FVector::CrossProduct, the same formula in either handedness
    inline constexpr DVec3 cross(DVec3 lhs, DVec3 rhs) noexcept {
        return {
            lhs.y * rhs.z - lhs.z * rhs.y,
            lhs.z * rhs.x - lhs.x * rhs.z,
            lhs.x * rhs.y - lhs.y * rhs.x
        };
    }

    inline constexpr f64 sizeSquared(DVec3 v) noexcept {
        return dot(v, v);
    }

    inline f64 size(DVec3 v) noexcept {
        return std::sqrt(dot(v, v));
    }

    inline constexpr DVec3 absolute(DVec3 v) noexcept {
        return {
            v.x < 0.0 ? -v.x : v.x,
            v.y < 0.0 ? -v.y : v.y,
            v.z < 0.0 ? -v.z : v.z
        };
    }

    // FVector::GetSafeNormal: zero below the tolerance, untouched when unit
    inline DVec3 getSafeNormal(DVec3 v, f64 tolerance = SmallNumber) noexcept {
        const auto squareSum = sizeSquared(v);
        if(squareSum == 1.0)
            return v;
        if(squareSum < tolerance)
            return {};

        return v * (1.0 / std::sqrt(squareSum));
    }

    // FVector::RotateAngleAxis, about a unit axis
    inline DVec3 rotateAngleAxis(DVec3 v, f64 angleDeg, DVec3 axis) noexcept {
        const auto radians = angleDeg * (std::numbers::pi / 180.0);
        const auto s = std::sin(radians);
        const auto c = std::cos(radians);
        const auto xx = axis.x * axis.x;
        const auto yy = axis.y * axis.y;
        const auto zz = axis.z * axis.z;
        const auto xy = axis.x * axis.y;
        const auto yz = axis.y * axis.z;
        const auto zx = axis.z * axis.x;
        const auto xs = axis.x * s;
        const auto ys = axis.y * s;
        const auto zs = axis.z * s;
        const auto omc = 1.0 - c;

        return {
            (omc * xx + c) * v.x + (omc * xy - zs) * v.y +
                (omc * zx + ys) * v.z,
            (omc * xy + zs) * v.x + (omc * yy + c) * v.y +
                (omc * yz - xs) * v.z,
            (omc * zx - ys) * v.x + (omc * yz + xs) * v.y + (omc * zz + c) * v.z
        };
    }

    inline constexpr Vec3 toVec3(DVec3 v) noexcept {
        return {
            static_cast<f32>(v.x),
            static_cast<f32>(v.y),
            static_cast<f32>(v.z)
        };
    }

    inline constexpr DVec3 toDVec3(Vec3 v) noexcept {
        return {v.x, v.y, v.z};
    }

    // FVector2D
    struct Vec2d {
        f64 x = 0.0;
        f64 y = 0.0;
    };

    // FIntPoint
    struct IntPoint {
        i32 x = 0;
        i32 y = 0;

        friend constexpr bool operator==(IntPoint, IntPoint) = default;
    };

    // FIntRect: min inclusive, max exclusive
    struct IntRect {
        IntPoint min;
        IntPoint max;

        constexpr i32 Width() const noexcept { return max.x - min.x; }
        constexpr i32 Height() const noexcept { return max.y - min.y; }
        constexpr i32 Area() const noexcept { return Width() * Height(); }
        constexpr bool IsEmpty() const noexcept {
            return Width() <= 0 || Height() <= 0;
        }

        // FIntRect::Clip
        constexpr void Clip(const IntRect& bounds) noexcept {
            min.x = std::max(min.x, bounds.min.x);
            min.y = std::max(min.y, bounds.min.y);
            max.x = std::min(max.x, bounds.max.x);
            max.y = std::min(max.y, bounds.max.y);
            max.x = std::max(min.x, max.x);
            max.y = std::max(min.y, max.y);
        }

        friend constexpr bool operator==(IntRect, IntRect) = default;
    };

    // FBox: invalid until a point is added
    struct Box3d {
        DVec3 min;
        DVec3 max;
        bool valid = false;

        constexpr void Add(DVec3 p) noexcept {
            if(!valid) {
                min = p;
                max = p;
                valid = true;
                return;
            }
            for(usize a = 0; a < 3; ++a) {
                min[a] = std::min(min[a], p[a]);
                max[a] = std::max(max[a], p[a]);
            }
        }

        constexpr DVec3 Size() const noexcept { return max - min; }
        constexpr DVec3 Center() const noexcept { return (min + max) * 0.5; }
        constexpr DVec3 Extent() const noexcept { return Size() * 0.5; }
    };

    // FQuat, only as far as a placed actor needs
    struct DQuat {
        f64 x = 0.0;
        f64 y = 0.0;
        f64 z = 0.0;
        f64 w = 1.0;

        // FRotator::Quaternion: pitch about Y, yaw about Z, roll about X
        static DQuat FromRotator(f64 pitch, f64 yaw, f64 roll) noexcept {
            constexpr auto HalfRadians = std::numbers::pi / 360.0;
            const auto sp = std::sin(pitch * HalfRadians);
            const auto cp = std::cos(pitch * HalfRadians);
            const auto sy = std::sin(yaw * HalfRadians);
            const auto cy = std::cos(yaw * HalfRadians);
            const auto sr = std::sin(roll * HalfRadians);
            const auto cr = std::cos(roll * HalfRadians);

            return DQuat{
                .x = cr * sp * sy - sr * cp * cy,
                .y = -cr * sp * cy - sr * cp * sy,
                .z = cr * cp * sy - sr * sp * cy,
                .w = cr * cp * cy + sr * sp * sy
            };
        }

        // FQuat::RotateVector
        constexpr DVec3 Rotate(DVec3 v) const noexcept {
            const DVec3 q{x, y, z};
            const auto tt = 2.0 * cross(q, v);

            return v + w * tt + cross(q, tt);
        }

        // FQuat::UnrotateVector
        constexpr DVec3 Unrotate(DVec3 v) const noexcept {
            const DVec3 q{-x, -y, -z};
            const auto tt = 2.0 * cross(q, v);

            return v + w * tt + cross(q, tt);
        }
    };

    // FTransform: scale, then rotation, then translation
    struct MintTransform {
        DQuat rotation;
        DVec3 translation;
        DVec3 scale{1.0, 1.0, 1.0};

        constexpr DVec3 TransformPosition(DVec3 p) const noexcept {
            return rotation.Rotate(scale * p) + translation;
        }

        constexpr DVec3 TransformVectorNoScale(DVec3 v) const noexcept {
            return rotation.Rotate(v);
        }

        constexpr DVec3 InverseTransformPositionNoScale(
            DVec3 p
        ) const noexcept {
            return rotation.Unrotate(p - translation);
        }

        constexpr DVec3 InverseTransformVectorNoScale(DVec3 v) const noexcept {
            return rotation.Unrotate(v);
        }

        // local to Mint world, as a column-major matrix
        constexpr Mat4 ToMat4() const noexcept {
            const auto ex = rotation.Rotate({scale.x, 0.0, 0.0});
            const auto ey = rotation.Rotate({0.0, scale.y, 0.0});
            const auto ez = rotation.Rotate({0.0, 0.0, scale.z});

            return Mat4{
                Vec4{
                    static_cast<f32>(ex.x),
                    static_cast<f32>(ex.y),
                    static_cast<f32>(ex.z),
                    0.0f
                },
                Vec4{
                    static_cast<f32>(ey.x),
                    static_cast<f32>(ey.y),
                    static_cast<f32>(ey.z),
                    0.0f
                },
                Vec4{
                    static_cast<f32>(ez.x),
                    static_cast<f32>(ez.y),
                    static_cast<f32>(ez.z),
                    0.0f
                },
                Vec4{
                    static_cast<f32>(translation.x),
                    static_cast<f32>(translation.y),
                    static_cast<f32>(translation.z),
                    1.0f
                }
            };
        }
    };

    // Mint world to Crowy world: crowy = 0.01 (mint.y, mint.z, mint.x). A
    // cyclic permutation, so handedness and winding survive it.
    inline constexpr Mat4 mintToCrowy() noexcept {
        constexpr auto S = static_cast<f32>(MintCmToCrowyM);

        return Mat4{
            Vec4{0.0f, 0.0f, S, 0.0f},
            Vec4{S, 0.0f, 0.0f, 0.0f},
            Vec4{0.0f, S, 0.0f, 0.0f},
            Vec4{0.0f, 0.0f, 0.0f, 1.0f}
        };
    }

    inline constexpr Vec3 toCrowyVector(DVec3 v) noexcept {
        return toVec3(DVec3{v.y, v.z, v.x});
    }

    inline constexpr Vec3 toCrowyPoint(DVec3 p) noexcept {
        return toCrowyVector(p * MintCmToCrowyM);
    }

    inline constexpr DVec3 fromCrowyVector(Vec3 v) noexcept {
        return {v.z, v.x, v.y};
    }

    inline constexpr DVec3 fromCrowyPoint(Vec3 p) noexcept {
        return fromCrowyVector(p) * (1.0 / MintCmToCrowyM);
    }
}
