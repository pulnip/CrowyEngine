#include <vector>

#include <gtest/gtest.h>

#include "Geometry/Overlap3D.hpp"
#include "MeshData.hpp"
#include "MeshGenerator.hpp"

using namespace Crowy;

namespace
{
    void expectNear(Vec3 actual, Vec3 expected) {
        EXPECT_NEAR(actual.x, expected.x, 1e-6f);
        EXPECT_NEAR(actual.y, expected.y, 1e-6f);
        EXPECT_NEAR(actual.z, expected.z, 1e-6f);
    }
}

TEST(MeshBounds, AUnitBoxIsHalfAMetreEachWay) {
    const auto bounds = boundsOf(MakeBox(0.5f).vertices);

    expectNear(bounds.center, Vec3{0.0f, 0.0f, 0.0f});
    expectNear(bounds.halfScale, Vec3{0.5f, 0.5f, 0.5f});
}

TEST(MeshBounds, ASphereReachesItsRadiusOnEveryAxis) {
    const auto bounds = boundsOf(MakeSphere(0.5f, 32, 16).vertices);

    expectNear(bounds.center, Vec3{0.0f, 0.0f, 0.0f});
    expectNear(bounds.halfScale, Vec3{0.5f, 0.5f, 0.5f});
}

TEST(MeshBounds, AnOffCentreMeshKeepsItsCentre) {
    std::vector<Vertex> vertices(2);
    vertices[0].position = Vec3{1.0f, 2.0f, 3.0f};
    vertices[1].position = Vec3{3.0f, 2.0f, -1.0f};

    const auto bounds = boundsOf(vertices);
    expectNear(bounds.center, Vec3{2.0f, 2.0f, 1.0f});
    expectNear(bounds.halfScale, Vec3{1.0f, 0.0f, 2.0f});
}

TEST(MeshBounds, NoVerticesHaveNoExtent) {
    const auto bounds = boundsOf(std::vector<Vertex>{});

    expectNear(bounds.halfScale, Vec3{0.0f, 0.0f, 0.0f});
}
