#pragma once

#include <span>
#include "GeoUtil.hpp"
#include "Primitives.hpp"
#include "LinearAlgebra.hpp"

namespace Crowy
{
    // GJK2D
    namespace detail{
        inline constexpr auto support(
            const std::span<const Vec2> polygon,
            const Vec2 v
        ) noexcept{
            auto point = polygon[0];
            f32 max_product = dot(point, v);

            for(usize i=1; i<polygon.size(); ++i){
                f32 product = dot(polygon[i], v);
                if(product > max_product){
                    point = polygon[i];
                    max_product = product;
                }
            }

            return point;
        }

        inline constexpr auto mink_dif(
            const std::span<const Vec2> polygon0,
            const std::span<const Vec2> polygon1,
            const Vec2 dir
        ) noexcept{
            const auto poly0_sup = support(polygon0,  dir);
            const auto poly1_sup = support(polygon1, -dir);
            return poly0_sup - poly1_sup;
        }

        inline constexpr auto normal(const Vec2 v, const Vec2 dir) noexcept{
            const auto candidate0 = perp(v);
            const auto candidate1 = perp(-v);

            if(dot(candidate0, dir) > 0)
                return candidate0;
            else
                return candidate1;
        }

        inline constexpr Vec2 barycenter(const std::span<const Vec2> polygon) noexcept{
            if(polygon.empty())
                return {0, 0};

            Vec2 sum = polygon[0];
            for(usize i=1; i<polygon.size(); ++i)
                sum = sum + polygon[i];

            return sum / polygon.size();
        }
    }

    inline constexpr bool GJK2D(
        const std::span<const Vec2> polygon0, const std::span<const Vec2> polygon1,
        const f32 epsilon = 0.0f
    ) noexcept{
        if(polygon0.data() == polygon1.data())
            return true;

        if((polygon0.size() < 1) || (polygon1.size() < 1))
            return false;
        if((polygon0.size() == 1) && (polygon1.size() == 1))
            return polygon0[0]==polygon1[0];

        using detail::barycenter;
        using detail::mink_dif;

        Vec2 zero_dir_normal = barycenter(polygon0) - barycenter(polygon1);
        if( abs(zero_dir_normal.x) <= epsilon &&
            abs(zero_dir_normal.y) <= epsilon
        ){
            // fallback
            zero_dir_normal = Vec2{1, 0};
        }

        auto point0 = mink_dif(polygon0, polygon1, zero_dir_normal);
        auto point1 = mink_dif(polygon0, polygon1, -point0);

        const f32 initial_dotted = dot(point1-point0, point1);

        if(initial_dotted < 0)
            return false;
        else if(initial_dotted == 0)
            return true;

        using detail::normal;

        const usize max_iter = polygon0.size() + polygon1.size() + 5;
        usize iter = 0;
        while(true){
            // normal vector of point1-point2 (Origin direction)
            zero_dir_normal = normal(point1-point0, -point0);
            // equivalent to zero_dir_normal = normal(point1-point2, -point2);
            const Vec2 point2 = mink_dif(polygon0, polygon1, zero_dir_normal);
            if(dot(zero_dir_normal, point2) < 0)
                return false;

            if( (std::abs(point2.x - point0.x) < epsilon && std::abs(point2.y - point0.y) < epsilon) ||
                (std::abs(point2.x - point1.x) < epsilon && std::abs(point2.y - point1.y) < epsilon)
            ){
                return false;
            }

            const auto c1 = cross(point0-point2, -point2);
            const auto c2 = cross(point1-point2, -point2);
            // outside of point1-point3 or point2-point3
            if(c1 * c2 > 0){
                if(std::abs(c1) < std::abs(c2))
                    point1 = point2;
                else
                    point0 = point2;
            }
            else
                return true;

            // safe guard about inf loop
            if(++iter > max_iter)
                break;
        }

        return false;
    }

    // GJK3D
    namespace detail{
        inline constexpr auto support(
            const std::span<const Vec3> polygon,
            const Vec3 v
        ) noexcept{
            auto point = polygon[0];
            f32 max_product = dot(point, v);

            for(usize i=1; i<polygon.size(); ++i){
                f32 product = dot(polygon[i], v);
                if(product > max_product){
                    point = polygon[i];
                    max_product = product;
                }
            }

            return point;
        }

        inline constexpr auto mink_dif(
            const std::span<const Vec3> polygon0,
            const std::span<const Vec3> polygon1,
            const Vec3 dir
        ) noexcept{
            const auto poly0_sup = support(polygon0,  dir);
            const auto poly1_sup = support(polygon1, -dir);

            return poly0_sup - poly1_sup;
        }

        inline constexpr auto dir_normal(
            const Vec3 p0, const Vec3 p1, const Vec3 p2,
            const Vec3 tgt_pt
        ) noexcept{
            // plane equation: dot(n, x) + d = 0
            const auto n = cross(p1-p0, p2-p0);
            const f32 d = -dot(n, p0);

            if(dot(n, tgt_pt) + d > 0)
                return n;
            else
                return -n;
        }

        inline constexpr Vec3 barycenter(const std::span<const Vec3> polygon) noexcept{
            if(polygon.empty())
                return zeros();

            Vec3 sum = polygon[0];
            for(usize i=1; i<polygon.size(); ++i)
                sum = sum + polygon[i];

            return sum / polygon.size();
        }

        inline constexpr bool inTriangle(
            const Vec3 point0, Vec3 point1, Vec3 point2,
            const Vec3 p_plane,
            const f32 epsilon = 0.0f
        ) noexcept{
            const auto n = cross(point1-point0, point2-point0);
            const auto eps2 = epsilon*epsilon;

            // p0-p1-p2 can make plane
            if(normSquared(n) > eps2){
                const auto c0 = cross(point1-point0, p_plane-point0);
                const auto c1 = cross(point2-point1, p_plane-point1);
                const auto c2 = cross(point0-point2, p_plane-point2);

                if( dot(c0, n) > 0 &&
                    dot(c1, n) > 0 &&
                    dot(c2, n) > 0
                )
                    return true;
                return false;
            }

            const auto dist12_sq = normSquared(point2-point1);
            const auto dist20_sq = normSquared(point0-point2);
            const auto dist01_sq = normSquared(point1-point0);
            // p0-p1-p2 cannot make line
            if( dist12_sq < eps2 &&
                dist20_sq < eps2 &&
                dist01_sq < eps2
            ){
                if(normSquared(point0-p_plane) < eps2)
                    return true;
                return false;
            }

            if(dist01_sq < eps2)
                point1 = point2;
            // now, point0-point1 can make line.
            Vec3 u = point1 - point0;
            f32 t_sq = normSquared(p_plane - point0) / normSquared(u);
            if(0 <= t_sq && t_sq <= 1)
                return true;
            return false;
        }
    }

    inline constexpr bool GJK3D(
        const std::span<const Vec3> polygon0, const std::span<const Vec3> polygon1,
        const f32 epsilon = 0.0f
    ) noexcept{
        if(polygon0.data() == polygon1.data())
            return true;

        if((polygon0.size() < 1) || (polygon1.size() < 1))
            return false;
        if((polygon0.size() == 1) && (polygon1.size()==1))
            return polygon0[0]==polygon1[0];

        using detail::barycenter;
        using detail::mink_dif;

        const auto eps2 = epsilon*epsilon;

        auto zero_dir_normal = barycenter(polygon0);
            - barycenter(polygon1);
        if( abs(zero_dir_normal.x) <= epsilon &&
            abs(zero_dir_normal.y) <= epsilon &&
            abs(zero_dir_normal.z) <= epsilon
        ){
            // fallback
            zero_dir_normal = unitX();
        }

        auto point0 = mink_dif(polygon0, polygon1, zero_dir_normal);
        auto point1 = mink_dif(polygon0, polygon1, -point0);

        // normal vector of p0-p1 with Origin direction
        Vec3 op0p1_up_dir = cross(
            cross(point1-point0, -point0),
            point1-point0
        );
        if(normSquared(op0p1_up_dir) < eps2)
            op0p1_up_dir = -point1;

        Vec3 point2 = mink_dif(polygon0, polygon1, op0p1_up_dir);

        using detail::dir_normal;
        using detail::inTriangle;

        const usize max_iter = polygon0.size() + polygon1.size() + 10;
        usize iter = 0;
        while(true){
            // simplex boundary check for p0-p1-p2 plane
            zero_dir_normal = dir_normal(point0, point1, point2, zeros());
            if(normSquared(zero_dir_normal) < eps2){
                return inTriangle(point0, point1, point2, zeros());
            }
            // if(inTriangle(point0, point1, point2, zeros()))
            //     return true;

            const Vec3 point3 = mink_dif(polygon0, polygon1, zero_dir_normal);
            // plane-point3 distance is shorter than
            // plane-Origin distance
            if(dot(zero_dir_normal, point3) < 0)
                return false;

            // <-- current state -->
            //
            //         *  <- Point 3
            //
            //         O
            //
            //         /\   ^ plane normal dir
            //        /  \      (towards Origin)
            //       /____\ <- p0p1p2 plane
            // <------------------->

            // simplex boundary check for new point3
            const auto n0 = dir_normal(point1, point2, point3, zeros());
            if(inTriangle(point1, point2, point3, zeros()))
                return true;

            const auto n1 = dir_normal(point0, point2, point3, zeros());
            if(inTriangle(point0, point2, point3, zeros()))
                return true;

            const auto n2 = dir_normal(point0, point1, point3, zeros());
            if(inTriangle(point0, point1, point3, zeros()))
                return true;

            const auto n0_same_dir = dot(zero_dir_normal, n0) > 0;
            const auto n1_same_dir = dot(zero_dir_normal, n1) > 0;
            const auto n2_same_dir = dot(zero_dir_normal, n2) > 0;
            // Origin is located outside of p1-p2-p3 plane
            if(n0_same_dir)
                point0 = point3;
            // Origin is located outside of p0-p2-p3 plane
            else if(n1_same_dir)
                point1 = point3;
            // Origin is located outside of p0-p1-p3 plane
            else if(n2_same_dir)
                point2 = point3;
            // Origin is located inside of simplex
            else
                return true;

            // safe guard about inf loop
            if(++iter > max_iter)
                break;
        }

        return false;
    }
}
