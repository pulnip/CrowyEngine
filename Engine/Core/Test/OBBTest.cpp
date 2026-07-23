#include <cmath>
#include <gtest/gtest.h>
#include "Geometry/Overlap3D.hpp"
#include "Primitives.hpp"

using namespace Crowy;

TEST(Overlap, Overlapped){
    static_assert(overlap(0.0f, 2.0f, 1.0f, 3.0f));
}

TEST(Oberlap, NonOverlapped){
    static_assert(!overlap(0.0f, 1.0f, 2.0f, 3.0f));
}

TEST(Overlap, Boundary){
    static_assert(overlap(0.0f, 1.0f, 1.0f, 2.0f));
}

static constexpr Vec3 AXIS_X{2.0f, 0.0f, 0.0f};
static constexpr Vec3 AXIS_Y{0.0f, 2.0f, 0.0f};
static constexpr Vec3 AXIS_Z{0.0f, 0.0f, 2.0f};

TEST(OBB3D, EquivalentOverlapped){
    static_assert(OverlapOBB3D(
        OBB3D{
            .center = zeros(),
            .halfAxes = {
                unitX(),
                unitY(),
                unitZ()
            }
        },
        OBB3D{
            .center = zeros(),
            .halfAxes = {
                unitX(),
                unitY(),
                unitZ()
            }
        })
    );
}

TEST(OBB3D, Overlapped){
    static_assert(OverlapOBB3D(
        OBB3D{
            .center = zeros(),
            .halfAxes = {
                unitX(),
                unitY(),
                unitZ()
            }
        },
        OBB3D{
            .center = ones(),
            .halfAxes = {
                unitX(),
                unitY(),
                unitZ()
            }
        })
    );
}

TEST(OBB3D, NonOverlapped){
    static_assert(!OverlapOBB3D(
        OBB3D{
            .center = zeros(),
            .halfAxes = {
                unitX(),
                unitY(),
                unitZ()
            }
        },
        OBB3D{
            .center = Vec3{5.0f, 0.0f, 0.0f},
            .halfAxes = {
                unitX(),
                unitY(),
                unitZ()
            }
        })
    );
}

TEST(OBB3D, Boundary) {
    static_assert(OverlapOBB3D(
        OBB3D{
            .center = zeros(),
            .halfAxes = {
                unitX(),
                unitY(),
                unitZ()
            }
        },
        OBB3D{
            .center = Vec3{2.0f, 0.0f, 0.0f},
            .halfAxes = {
                unitX(),
                unitY(),
                unitZ()
            }
        })
    );
}

TEST(OBB3D, RotatedOverlap) {
    constexpr Vec3 c1{0.0f, 0.0f, 0.0f};
    constexpr Vec3 c2{0.5f, 0.5f, 0.0f};

    const float s2 = std::sqrt(2.0f);
    const Vec3 rX{  s2,   s2, 0.0f};
    const Vec3 rY{ -s2,   s2, 0.0f};
    const Vec3 rZ{0.0f, 0.0f, 2.0f};

    EXPECT_TRUE(OverlapOBB3D(
        OBB3D{
            .center = c1,
            .halfAxes = {
                rX,
                rY,
                rZ
            }
        },
        OBB3D{
            .center = c2,
            .halfAxes = {
                rX,
                rY,
                rZ
            }
        })
    );
}

TEST(OBB3D, RotatedNonOverlap) {
    constexpr Vec3 c1{0.0f, 0.0f, 0.0f};
    constexpr Vec3 c2{3.0f, 3.0f, 0.0f};

    const float s2 = std::sqrt(2.0f);
    const Vec3 rX{  s2,   s2, 0.0f};
    const Vec3 rY{ -s2,   s2, 0.0f};
    const Vec3 rZ{0.0f, 0.0f, 2.0f};

    EXPECT_FALSE(OverlapOBB3D(
        OBB3D{
            .center = c1,
            .halfAxes = {
                rX,
                rY,
                rZ
            }
        },
        OBB3D{
            .center = c2,
            .halfAxes = {
                rX,
                rY,
                rZ
            }
        })
    );
}

TEST(OBB3D, RotatedBoundaryTouch) {
    constexpr Vec3 c1{         0.0f,          0.0f, 0.0f};
    constexpr Vec3 c2{1.4142135624f, 1.4142135624f, 0.0f};

    const float s2 = std::sqrt(2.0f);
    const Vec3 rX{  s2,   s2, 0.0f};
    const Vec3 rY{ -s2,   s2, 0.0f};
    const Vec3 rZ{0.0f, 0.0f, 2.0f};

    EXPECT_TRUE(OverlapOBB3D(
        OBB3D{
            .center = c1,
            .halfAxes = {
                rX,
                rY,
                rZ
            }
        },
        OBB3D{
            .center = c2,
            .halfAxes = {
                rX,
                rY,
                rZ
            }
        })
    );
}
