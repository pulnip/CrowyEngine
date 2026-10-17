#include <cstring>
#include <numeric>
#include <vector>

#include <gtest/gtest.h>

#include "FakeCommandList.hpp"
#include "FakeDevice.hpp"
#include "GeometryPool.hpp"
#include "RHICommandList.hpp"

using namespace Crowy;

namespace
{
    using Vertices = std::vector<Vertex>;
    using Indices = std::vector<u32>;

    Vertices vertices(usize count, f32 first) {
        Vertices out(count);
        for(usize i = 0; i < count; ++i)
            out[i].position.x = first + static_cast<f32>(i);
        return out;
    }

    Indices indices(usize count) {
        Indices out(count);
        std::iota(out.begin(), out.end(), 0u);
        return out;
    }

    class GeometryPoolTest: public testing::Test {
    protected:
        FakeDevice device;
        GeometryPool pool{device, 64, 256};
        FakeCommandList cmdList;

        RHIBufferBarrier VertexRelease(RHIResourceUsage read) {
            return MakeBarrier(
                pool.GetVertexBuffer(),
                RHIResourceUsage::CopyDst,
                read
            );
        }

        RHIBufferBarrier IndexRelease() {
            return MakeBarrier(
                pool.GetIndexBuffer(),
                RHIResourceUsage::CopyDst,
                RHIResourceUsage::IndexBuffer
            );
        }
    };
}

TEST_F(GeometryPoolTest, TheFirstUploadTakesTheBuffersFresh) {
    const auto mesh = vertices(3, 0.0f);
    const auto tris = indices(3);
    ASSERT_TRUE(pool.Add(mesh, tris));

    const auto releases = pool.RecordUploads(cmdList);

    ASSERT_EQ(cmdList.blitPasses.size(), 1u);
    const auto& pass = cmdList.blitPasses[0];
    EXPECT_EQ(
        pass.bufferAcquires,
        (FakeCommandList::BufferBarriers{
            MakeBarrier(
                pool.GetVertexBuffer(),
                RHIResourceUsage::Undefined,
                RHIResourceUsage::CopyDst
            ),
            MakeBarrier(
                pool.GetIndexBuffer(),
                RHIResourceUsage::Undefined,
                RHIResourceUsage::CopyDst
            ),
        })
    );
    EXPECT_TRUE(cmdList.violations.empty());
    ASSERT_EQ(releases.size(), 2u);
}

// vertex pulling reads the vertices as a buffer in the vertex stage
TEST_F(GeometryPoolTest, TheVerticesGoToTheReadTheDrawsMake) {
    ASSERT_TRUE(pool.Add(vertices(3, 0.0f), indices(3)));

    const auto releases = pool.RecordUploads(cmdList);
    ASSERT_EQ(releases.size(), 2u);
    EXPECT_EQ(releases[0], VertexRelease(RHIResourceUsage::SampledVertex));
    EXPECT_EQ(releases[1], IndexRelease());
    EXPECT_EQ(cmdList.blitPasses[0].bufferReleases[0], releases[0]);
}

TEST(GeometryPool, ABoundVertexBufferIsReleasedAsOne) {
    FakeDevice device;
    GeometryPool pool{device, 64, 256, RHIResourceUsage::VertexBuffer};
    FakeCommandList cmdList;
    ASSERT_TRUE(pool.Add(vertices(3, 0.0f), indices(3)));

    const auto releases = pool.RecordUploads(cmdList);
    ASSERT_EQ(releases.size(), 2u);
    EXPECT_EQ(
        releases[0],
        MakeBarrier(
            pool.GetVertexBuffer(),
            RHIResourceUsage::CopyDst,
            RHIResourceUsage::VertexBuffer
        )
    );
}

TEST_F(GeometryPoolTest, EachMeshIsCopiedWhereItWasPlaced) {
    ASSERT_TRUE(pool.Add(vertices(3, 0.0f), indices(3)));
    const auto second = vertices(4, 10.0f);
    const auto placed = pool.Add(second, indices(6)).value();

    pool.RecordUploads(cmdList);

    const auto& copies = cmdList.blitPasses[0].bufferCopies;
    ASSERT_EQ(copies.size(), 4u);

    const auto& vertexCopy = copies[2];
    EXPECT_EQ(vertexCopy.dst, &pool.GetVertexBuffer());
    EXPECT_EQ(
        vertexCopy.dstOffset,
        static_cast<usize>(placed.baseVertex) * sizeof(Vertex)
    );
    EXPECT_EQ(vertexCopy.size, second.size() * sizeof(Vertex));
    EXPECT_EQ(
        std::memcmp(
            device.transient.bytes.data() + vertexCopy.srcOffset,
            second.data(),
            vertexCopy.size
        ),
        0
    );

    const auto& indexCopy = copies[3];
    EXPECT_EQ(indexCopy.dst, &pool.GetIndexBuffer());
    EXPECT_EQ(indexCopy.dstOffset, placed.firstIndex * sizeof(u32));
    EXPECT_EQ(placed.indexCount, 6u);
}

TEST_F(GeometryPoolTest, NothingQueuedRecordsNothing) {
    EXPECT_TRUE(pool.RecordUploads(cmdList).empty());
    EXPECT_TRUE(cmdList.blitPasses.empty());
}

TEST_F(GeometryPoolTest, AMeshPastTheVertexSpaceIsAnError) {
    const auto result = pool.Add(vertices(65, 0.0f), indices(3));

    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().starts_with("geometry pool out of vertex space"))
        << result.error();
}

// the vertices found room before the indices did not, and give it back
TEST_F(GeometryPoolTest, AMeshPastTheIndexSpaceTakesNoVertexSpace) {
    const auto result = pool.Add(vertices(64, 0.0f), indices(257));
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().starts_with("geometry pool out of index space"))
        << result.error();

    EXPECT_TRUE(pool.Add(vertices(64, 0.0f), indices(3)).has_value());
}
