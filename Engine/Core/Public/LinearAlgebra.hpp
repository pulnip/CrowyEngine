#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <type_traits>

#include "Primitives.hpp"

namespace Crowy
{
    // column-major matrix
    struct Mat4 {
        std::array<Vec4, 4> columns;

        constexpr Vec4& operator[](usize column) noexcept {
            return columns[column];
        }
        constexpr const Vec4& operator[](usize column) const noexcept {
            return columns[column];
        }

        constexpr bool operator==(const Mat4&) const noexcept = default;
    };
    static_assert(sizeof(Mat4) == 64);
#ifdef __cpp_lib_is_implicit_lifetime
    static_assert(std::is_implicit_lifetime_v<Mat4>);
#else
    static_assert(std::is_trivially_copyable_v<Mat4>);
#endif
    static_assert(std::is_standard_layout_v<Mat4>);

    inline constexpr f32 dot(Vec2 lhs, Vec2 rhs) noexcept {
        return lhs.x * rhs.x + lhs.y * rhs.y;
    }
    inline constexpr f32 normSquared(Vec2 v) noexcept {
        return dot(v, v);
    }
    inline f32 norm(Vec2 v) noexcept {
        return std::sqrt(normSquared(v));
    }
    inline Vec2 normalize(Vec2 v) noexcept {
        return v / norm(v);
    }
    inline constexpr f32 cross(Vec2 lhs, Vec2 rhs) noexcept {
        return lhs.x * rhs.y - lhs.y * rhs.x;
    }

    inline constexpr f32 dot(Vec3 lhs, Vec3 rhs) noexcept {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
    }
    inline constexpr f32 normSquared(Vec3 v) noexcept {
        return dot(v, v);
    }
    inline f32 norm(Vec3 v) noexcept {
        return std::sqrt(normSquared(v));
    }
    inline Vec3 normalize(Vec3 v) noexcept {
        return v / norm(v);
    }
    inline constexpr Vec3 cross(Vec3 lhs, Vec3 rhs) noexcept {
        return {
            lhs.y * rhs.z - lhs.z * rhs.y,
            lhs.z * rhs.x - lhs.x * rhs.z,
            lhs.x * rhs.y - lhs.y * rhs.x
        };
    }
    inline constexpr Vec3 projectOntoPlane(Vec3 v, Vec3 unitNormal) noexcept {
        return v - unitNormal * dot(unitNormal, v);
    }
    // radians from `from` to `to`, both across `axis`, signed by it
    inline f32 signedAngle(Vec3 from, Vec3 to, Vec3 axis) noexcept {
        return std::atan2(dot(axis, cross(from, to)), dot(from, to));
    }

    inline constexpr f32 dot(Vec4 lhs, Vec4 rhs) noexcept {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
    }
    inline constexpr f32 normSquared(Vec4 v) noexcept {
        return dot(v, v);
    }
    inline f32 norm(Vec4 v) noexcept {
        return std::sqrt(normSquared(v));
    }
    inline Vec4 normalize(Vec4 v) noexcept {
        return v / norm(v);
    }

    inline Vec4 quat(Vec3 r, Vec3 u, Vec3 f) noexcept {
        f32 m00 = r.x, m01 = u.x, m02 = f.x;
        f32 m10 = r.y, m11 = u.y, m12 = f.y;
        f32 m20 = r.z, m21 = u.z, m22 = f.z;

        f32 trace = m00 + m11 + m22;
        Vec4 q;

        if(trace > 0.0f) {
            f32 s = std::sqrt(trace + 1.0f) * 2.0f;
            q.x = (m21 - m12) / s;
            q.y = (m02 - m20) / s;
            q.z = (m10 - m01) / s;
            q.w = 0.25f * s;
        }
        else if((m00 > m11) && (m00 > m22)) {
            f32 s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
            q.x = 0.25f * s;
            q.y = (m01 + m10) / s;
            q.z = (m02 + m20) / s;
            q.w = (m21 - m12) / s;
        }
        else if(m11 > m22) {
            f32 s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
            q.x = (m01 + m10) / s;
            q.y = 0.25f * s;
            q.z = (m12 + m21) / s;
            q.w = (m02 - m20) / s;
        }
        else {
            f32 s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f; // s=4*q.z
            q.x = (m02 + m20) / s;
            q.y = (m12 + m21) / s;
            q.z = 0.25f * s;
            q.w = (m10 - m01) / s;
        }

        return normalize(q);
    }

    inline Vec4 rotateX(f32 theta) noexcept {
        f32 half = theta * 0.5f;
        return {std::sin(half), 0.0f, 0.0f, std::cos(half)};
    }
    inline Vec4 rotateY(f32 theta) noexcept {
        f32 half = theta * 0.5f;
        return {0.0f, std::sin(half), 0.0f, std::cos(half)};
    }
    inline Vec4 rotateZ(f32 theta) noexcept {
        f32 half = theta * 0.5f;
        return {0.0f, 0.0f, std::sin(half), std::cos(half)};
    }

    inline auto axisAngle(Vec3 axis, f32 radian) noexcept {
        auto half = radian / 2;
        f32 s = std::sin(half);
        return Vec4{axis.x * s, axis.y * s, axis.z * s, std::cos(half)};
    }

    // radians, x pitch, y yaw, z roll, composed as the fly camera's look:
    // rotateY(y) * rotateX(x) * rotateZ(z)
    inline Vec4 quatOf(Vec3 euler) noexcept {
        return quat(rotateY(euler.y), quat(rotateX(euler.x), rotateZ(euler.z)));
    }

    // quatOf's inverse; the pitch stays within +-pi/2, and at either end the
    // roll folds into the yaw
    inline Vec3 eulerOf(Vec4 q) noexcept {
        const auto m12 = 2.0f * (q.y * q.z - q.w * q.x);
        const auto pitch = std::asin(std::clamp(-m12, -1.0f, 1.0f));
        if(std::abs(m12) > 1.0f - 1e-6f) {
            const auto m00 = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
            const auto m20 = 2.0f * (q.x * q.z - q.w * q.y);
            return Vec3{pitch, std::atan2(-m20, m00), 0.0f};
        }

        const auto m02 = 2.0f * (q.x * q.z + q.w * q.y);
        const auto m22 = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
        const auto m10 = 2.0f * (q.x * q.y + q.w * q.z);
        const auto m11 = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);

        return Vec3{pitch, std::atan2(m02, m22), std::atan2(m10, m11)};
    }

    // turns +Z to the unit `direction` with no roll: the fly camera's yaw and
    // pitch, pitch positive looking down
    inline Vec4 lookRotation(Vec3 direction) noexcept {
        const auto yaw = std::atan2(direction.x, direction.z);
        const auto pitch = std::asin(std::clamp(-direction.y, -1.0f, 1.0f));

        return quatOf(Vec3{pitch, yaw, 0.0f});
    }

    inline constexpr auto right(Vec4 q) noexcept {
        auto e_x = Vec4{1.0f, 0.0f, 0.0f, 0.0f};
        auto r = rotate(e_x, q);
        return static_cast<Vec3>(r);
    }
    inline constexpr auto groundRight(Vec4 quat) noexcept {
        return projectOntoPlane(right(quat), unitY());
    }
    inline constexpr auto up(Vec4 q) noexcept {
        auto e_y = Vec4{0.0f, 1.0f, 0.0f, 0.0f};
        auto r = rotate(e_y, q);
        return static_cast<Vec3>(r);
    }
    inline constexpr auto forward(Vec4 q) noexcept {
        auto e_z = Vec4{0.0f, 0.0f, 1.0f, 0.0f};
        auto r = rotate(e_z, q);
        return static_cast<Vec3>(r);
    }
    inline constexpr auto groundForward(Vec4 quat) noexcept {
        return projectOntoPlane(forward(quat), unitY());
    }

    inline constexpr Mat4 unitMat() noexcept {
        return {
            Vec4{1.0f, 0.0f, 0.0f, 0.0f},
            Vec4{0.0f, 1.0f, 0.0f, 0.0f},
            Vec4{0.0f, 0.0f, 1.0f, 0.0f},
            Vec4{0.0f, 0.0f, 0.0f, 1.0f}
        };
    }

    inline constexpr Mat4 transpose(const Mat4 mat) noexcept {
        return Mat4{
            Vec4{mat[0][0], mat[1][0], mat[2][0], mat[3][0]},
            Vec4{mat[0][1], mat[1][1], mat[2][1], mat[3][1]},
            Vec4{mat[0][2], mat[1][2], mat[2][2], mat[3][2]},
            Vec4{mat[0][3], mat[1][3], mat[2][3], mat[3][3]}
        };
    }

    // inverse for Mat4 without Scale (= Rotation + Translation Only)
    inline constexpr Mat4 inverseRigid(const Mat4 mat) noexcept {
        const auto x = static_cast<Vec3>(mat[0]);
        const auto y = static_cast<Vec3>(mat[1]);
        const auto z = static_cast<Vec3>(mat[2]);
        const auto t = static_cast<Vec3>(mat[3]);
        const auto tx = -dot(x, t);
        const auto ty = -dot(y, t);
        const auto tz = -dot(z, t);

        return Mat4{
            Vec4{x.x, y.x, z.x, 0.0f},
            Vec4{x.y, y.y, z.y, 0.0f},
            Vec4{x.z, y.z, z.z, 0.0f},
            Vec4{tx, ty, tz, 1.0f}
        };
    }

    // inverse for Mat4 with scale, but no projection (last row is 0 0 0 1)
    inline constexpr Mat4 inverseAffine(const Mat4 mat) noexcept {
        const auto x = static_cast<Vec3>(mat[0]);
        const auto y = static_cast<Vec3>(mat[1]);
        const auto z = static_cast<Vec3>(mat[2]);
        const auto t = static_cast<Vec3>(mat[3]);

        // rows of the inverted basis, adjugate over determinant
        const auto invDet = 1.0f / dot(x, cross(y, z));
        const auto r0 = cross(y, z) * invDet;
        const auto r1 = cross(z, x) * invDet;
        const auto r2 = cross(x, y) * invDet;

        return Mat4{
            Vec4{r0.x, r1.x, r2.x, 0.0f},
            Vec4{r0.y, r1.y, r2.y, 0.0f},
            Vec4{r0.z, r1.z, r2.z, 0.0f},
            Vec4{-dot(r0, t), -dot(r1, t), -dot(r2, t), 1.0f}
        };
    }

    inline constexpr Vec4 operator*(const Mat4& lhs, const Vec4& rhs) noexcept {
        return lhs[0] * rhs.x + lhs[1] * rhs.y + lhs[2] * rhs.z +
               lhs[3] * rhs.w;
    }

    inline constexpr auto operator*(const Mat4& lhs, const Mat4& rhs) noexcept {
        return Mat4{lhs * rhs[0], lhs * rhs[1], lhs * rhs[2], lhs * rhs[3]};
    }

    // affine matrices only: both drop the w
    inline constexpr Vec3 transformPoint(const Mat4& m, Vec3 p) noexcept {
        return static_cast<Vec3>(m * toVec4(p, 1.0f));
    }
    inline constexpr Vec3 transformDirection(const Mat4& m, Vec3 d) noexcept {
        return static_cast<Vec3>(m * toVec4(d, 0.0f));
    }

    // left-handed
    inline auto lookAt(Vec3 eye, Vec3 target, Vec3 up) noexcept {
        auto f = normalize(target - eye);
        auto r = normalize(cross(up, f));
        auto u = cross(f, r);
        return Mat4{
            Vec4{r.x, u.x, f.x, 0.0f},
            Vec4{r.y, u.y, f.y, 0.0f},
            Vec4{r.z, u.z, f.z, 0.0f},
            Vec4{-dot(r, eye), -dot(u, eye), -dot(f, eye), 1.0f}
        };
    }

    inline auto rotateXMat(f32 theta) noexcept {
        f32 c = std::cos(theta);
        f32 s = std::sin(theta);
        return Mat4{
            Vec4{1.0f, 0.0f, 0.0f, 0.0f},
            Vec4{0.0f, c, s, 0.0f},
            Vec4{0.0f, -s, c, 0.0f},
            Vec4{0.0f, 0.0f, 0.0f, 1.0f}
        };
    }

    inline auto rotateYMat(f32 theta) noexcept {
        f32 c = std::cos(theta);
        f32 s = std::sin(theta);
        return Mat4{
            Vec4{c, 0.0f, -s, 0.0f},
            Vec4{0.0f, 1.0f, 0.0f, 0.0f},
            Vec4{s, 0.0f, c, 0.0f},
            Vec4{0.0f, 0.0f, 0.0f, 1.0f}
        };
    }

    inline auto rotateZMat(f32 theta) noexcept {
        f32 c = std::cos(theta);
        f32 s = std::sin(theta);
        return Mat4{
            Vec4{c, s, 0.0f, 0.0f},
            Vec4{-s, c, 0.0f, 0.0f},
            Vec4{0.0f, 0.0f, 1.0f, 0.0f},
            Vec4{0.0f, 0.0f, 0.0f, 1.0f}
        };
    }

    inline constexpr auto translateMat(Vec3 t) noexcept {
        return Mat4{
            Vec4{1.0f, 0.0f, 0.0f, 0.0f},
            Vec4{0.0f, 1.0f, 0.0f, 0.0f},
            Vec4{0.0f, 0.0f, 1.0f, 0.0f},
            toVec4(t, 1.0f)
        };
    }

    inline constexpr auto rotateMat(Vec4 q) noexcept {
        f32 xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
        f32 xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
        f32 wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
        return Mat4{
            Vec4{1 - 2 * (yy + zz), 2 * (xy + wz), 2 * (xz - wy), 0.0f},
            Vec4{2 * (xy - wz), 1 - 2 * (xx + zz), 2 * (yz + wx), 0.0f},
            Vec4{2 * (xz + wy), 2 * (yz - wx), 1 - 2 * (xx + yy), 0.0f},
            Vec4{0.0f, 0.0f, 0.0f, 1.0f}
        };
    }

    inline constexpr auto scaleMat(Vec3 s) noexcept {
        return Mat4{
            Vec4{s.x, 0.0f, 0.0f, 0.0f},
            Vec4{0.0f, s.y, 0.0f, 0.0f},
            Vec4{0.0f, 0.0f, s.z, 0.0f},
            Vec4{0.0f, 0.0f, 0.0f, 1.0f}
        };
    }

    inline constexpr auto modelMat(Vec3 pos, Vec4 q, Vec3 scale) noexcept {
        auto t = translateMat(pos);
        auto r = rotateMat(q);
        auto s = scaleMat(scale);

        return t * r * s;
    }

    inline constexpr auto modelMat(Transform transform) noexcept {
        auto t = translateMat(transform.position);
        auto r = rotateMat(transform.rotation);
        auto s = scaleMat(transform.scale);

        return t * r * s;
    }

    inline constexpr auto viewMat(Vec3 pos, Vec4 q) noexcept {
        auto inv = conjugate(q);

        auto r = rotateMat(inv);
        auto t = translateMat(-pos);

        return r * t;
    }

    inline constexpr auto orthographic(
        f32 w,
        f32 h,
        f32 nearZ,
        f32 farZ
    ) noexcept {
        auto e22 = 1 / (farZ - nearZ);
        auto e23 = -nearZ * e22;

        return Mat4{
            Vec4{2 / w, 0.0f, 0.0f, 0.0f},
            Vec4{0.0f, 2 / h, 0.0f, 0.0f},
            Vec4{0.0f, 0.0f, e22, 0.0f},
            Vec4{0.0f, 0.0f, e23, 1.0f}
        };
    }

    // left-handed
    inline auto perspective(
        f32 fovY,
        f32 aspect,
        f32 nearZ,
        f32 farZ
    ) noexcept {
        auto tanHalfFovY = std::tan(0.5f * fovY);
        auto dz = farZ - nearZ;

        auto e00 = 1.0f / (aspect * tanHalfFovY);
        auto e11 = 1.0f / tanHalfFovY;
        auto e22 = farZ / dz;
        auto e23 = -(farZ * nearZ) / dz;

        return Mat4{
            Vec4{e00, 0.0f, 0.0f, 0.0f},
            Vec4{0.0f, e11, 0.0f, 0.0f},
            Vec4{0.0f, 0.0f, e22, 1.0f},
            Vec4{0.0f, 0.0f, e23, 0.0f}
        };
    }

    inline constexpr auto mvp(Mat4 model, Mat4 view, Mat4 proj) noexcept {
        // column-vector convention
        return proj * view * model;
    }

    inline constexpr f64 toRadian(f64 degree) noexcept {
        return degree * (std::numbers::pi / 180.0);
    }

    inline constexpr f64 toDegree(f64 radian) noexcept {
        return radian * (180.0 / std::numbers::pi);
    }

    // through f64: a multiply in f32 rounds differently
    inline constexpr f32 radiansOf(f32 degrees) noexcept {
        return static_cast<f32>(toRadian(degrees));
    }
    inline constexpr f32 degreesOf(f32 radians) noexcept {
        return static_cast<f32>(toDegree(radians));
    }

    inline f32 snapTo(f32 value, f32 step) noexcept {
        return std::round(value / step) * step;
    }

    // into [0, 360); a hair under 360 lands on 0, never on 360
    inline f32 wrapDegrees(f32 degrees) noexcept {
        const auto wrapped = degrees - 360.0f * std::floor(degrees / 360.0f);

        return wrapped >= 360.0f ? 0.0f : wrapped;
    }
}
