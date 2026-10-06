#include <gtest/gtest.h>

#include "PaintMeshes.hpp"

using namespace Crowy;

namespace
{
    // every triangle's winding agrees with its vertex normals, and the
    // normals point away from `inside`
    void expectOutward(const PaintMeshTriangles& mesh, DVec3 inside) {
        ASSERT_EQ(mesh.indices.size() % 3, 0u);
        for(usize t = 0; t < mesh.indices.size(); t += 3) {
            const auto a = toDVec3(mesh.positions[mesh.indices[t]]);
            const auto b = toDVec3(mesh.positions[mesh.indices[t + 1]]);
            const auto c = toDVec3(mesh.positions[mesh.indices[t + 2]]);
            const auto n = cross(b - a, c - a);
            const auto vertexNormal = toDVec3(mesh.normals[mesh.indices[t]]);

            EXPECT_GT(dot(n, vertexNormal), 0.0) << "triangle " << t / 3;
            EXPECT_GT(dot(n, (a + b + c) * (1.0 / 3.0) - inside), 0.0)
                << "triangle " << t / 3;
        }
    }
}

TEST(PaintMeshes, CubeIsTwelveOutwardTriangles) {
    const auto cube = makePaintCube(100.0);
    EXPECT_EQ(cube.indices.size(), 36u);
    EXPECT_EQ(cube.normals.size(), cube.positions.size());
    expectOutward(cube, {});

    const auto bounds = boundsOf(cube);
    EXPECT_EQ(bounds.min, (DVec3{-50.0, -50.0, -50.0}));
    EXPECT_EQ(bounds.max, (DVec3{50.0, 50.0, 50.0}));
}

TEST(PaintMeshes, WedgeRisesAlongX) {
    const auto wedge = makePaintWedge({500.0, 300.0, 150.0});
    const auto bounds = boundsOf(wedge);
    EXPECT_EQ(bounds.min, (DVec3{-250.0, -150.0, 0.0}));
    EXPECT_EQ(bounds.max, (DVec3{250.0, 150.0, 150.0}));
    expectOutward(wedge, {125.0, 0.0, 30.0});
}

TEST(PaintMeshes, SphereFacesOut) {
    const auto sphere = makePaintSphere(120.0, 32, 16);
    expectOutward(sphere, {});
    const auto bounds = boundsOf(sphere);
    EXPECT_NEAR(bounds.max.z, 120.0, 1e-4);
    EXPECT_NEAR(bounds.min.z, -120.0, 1e-4);
    // a pole row keeps one triangle per slice, the others two
    EXPECT_EQ(sphere.indices.size(), 3u * 32u * (2u * (16u - 2u) + 2u));
}
