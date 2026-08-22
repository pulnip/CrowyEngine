#include <numbers>
#include <gtest/gtest.h>
#include "Geometry/Frustum3D.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"

using namespace Crowy;

namespace{

    inline constexpr f32 FOV_Y = std::numbers::pi_v<f32> / 3;
    inline constexpr f32 NEAR_Z = 0.1f, FAR_Z = 300.0f;

    Frustum3D DefaultFrustum(Vec3 eye = zeros()){
        const auto view = viewMat(eye, unitQuat());
        const auto proj = perspective(FOV_Y, 1.0f, NEAR_Z, FAR_Z);

        return makeFrustum3D(proj * view);
    }

    AABB3D UnitBoxAt(Vec3 center){
        return AABB3D{
            .center = center,
            .halfScale = 0.5f * ones()
        };
    }
}

TEST(Frustum3D, IdentityIsTheClipBox){
    // An identity viewProj makes clip space the frustum
    // [-1,1] x [-1,1] x [0,1]
    const auto frustum = makeFrustum3D(unitMat());

    EXPECT_TRUE(OverlapFrustumAABB3D(frustum, UnitBoxAt({0.0f, 0.0f, 0.5f})));
    EXPECT_FALSE(OverlapFrustumAABB3D(frustum, UnitBoxAt({4.0f, 0.0f, 0.5f})));
    EXPECT_FALSE(OverlapFrustumAABB3D(frustum, UnitBoxAt({0.0f, -4.0f, 0.5f})));
    // behind the near plane: z = 0 in this convention, not z = -1
    EXPECT_FALSE(OverlapFrustumAABB3D(frustum, UnitBoxAt({0.0f, 0.0f, -4.0f})));
}

// Left-handed, so the camera looks down +Z.
TEST(Frustum3D, KeepsWhatIsInFront){
    const auto frustum = DefaultFrustum();

    EXPECT_TRUE(OverlapFrustumAABB3D(frustum, UnitBoxAt({0.0f, 0.0f, 10.0f})));
    EXPECT_TRUE(OverlapFrustumAABB3D(frustum, UnitBoxAt({0.0f, 0.0f, NEAR_Z * 2})));
}

TEST(Frustum3D, DropsWhatIsBehind){
    const auto frustum = DefaultFrustum();

    EXPECT_FALSE(OverlapFrustumAABB3D(frustum, UnitBoxAt({0.0f, 0.0f, -10.0f})));
    // clear of the near plane by half a unit; a box centred any closer than its
    // own half-extent straddles z = 0 and is kept on purpose
    EXPECT_FALSE(OverlapFrustumAABB3D(frustum, UnitBoxAt({0.0f, 0.0f, -1.0f})));
}

TEST(Frustum3D, DropsWhatIsBeyondFar){
    const auto frustum = DefaultFrustum();

    EXPECT_FALSE(OverlapFrustumAABB3D(frustum, UnitBoxAt({0.0f, 0.0f, FAR_Z + 10.0f})));
}

TEST(Frustum3D, DropsWhatIsOutsideTheCone){
    // 60 degrees vertical on a square target
    const auto frustum = DefaultFrustum();

    // the half-angle is 30 degrees,
    // so at z = 10 the frustum reaches +/- 10*tan(30) ~= 5.77 on both axes.
    EXPECT_TRUE(OverlapFrustumAABB3D(frustum, UnitBoxAt({5.0f, 0.0f, 10.0f})));
    EXPECT_FALSE(OverlapFrustumAABB3D(frustum, UnitBoxAt({8.0f, 0.0f, 10.0f})));
    EXPECT_TRUE(OverlapFrustumAABB3D(frustum, UnitBoxAt({0.0f, -5.0f, 10.0f})));
    EXPECT_FALSE(OverlapFrustumAABB3D(frustum, UnitBoxAt({0.0f, -8.0f, 10.0f})));
}

TEST(Frustum3D, KeepsBoxesStraddlingAPlane){
    const auto frustum = DefaultFrustum();

    // centred well outside the right plane at z = 10,
    // but wide enough to reach back into the frustum
    EXPECT_TRUE(OverlapFrustumAABB3D(frustum, AABB3D{
        .center = {12.0f, 0.0f, 10.0f},
        .halfScale = {8.0f, 1.0f, 1.0f}
    }));
    // and one that swallows the whole frustum
    EXPECT_TRUE(OverlapFrustumAABB3D(frustum, AABB3D{
        .center = zeros(),
        .halfScale = 1000.0f * ones()
    }));
}

TEST(Frustum3D, FollowsTheEye){
    const auto box = UnitBoxAt({0.0f, 0.0f, 10.0f});

    EXPECT_TRUE(OverlapFrustumAABB3D(DefaultFrustum(zeros()), box));
    // now standing past the box, still looking down +Z
    EXPECT_FALSE(OverlapFrustumAABB3D(DefaultFrustum({0.0f, 0.0f, 20.0f}), box));
}
