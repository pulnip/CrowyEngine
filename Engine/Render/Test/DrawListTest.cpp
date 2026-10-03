#include <array>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "DrawList.hpp"
#include "EnumUtil.hpp"
#include "FakeDevice.hpp"
#include "LinearAlgebra.hpp"
#include "PipelineCache.hpp"
#include "RHICommandList.hpp"
#include "RenderScene.hpp"
#include "SceneRenderer.hpp"

using namespace Crowy;

namespace
{
    using Batches = std::vector<DrawBatchIndexed>;
    using Rows = std::vector<DrawData>;

    constexpr std::array ColorFormats = {RHIPixelFormat::RGBA8_UNORM};

    // keeps what a list submits; nothing stands behind it
    class RecordingCommandList final: public RHICommandList {
    public:
        Batches batches;

        void BeginEvent(CStr) override {}
        void EndEvent() override {}
        void SetMarker(CStr) override {}
        void ExecuteIndirectIndexed(const DrawBatchIndexed& batch) override {
            batches.push_back(batch);
        }
    };

    // a fragment entry per pipeline, so two entries key two pipelines
    MaterialPipelineDesc Pipeline(
        CStr fragmentEntry,
        MaterialDomain domain = MaterialDomain::Opaque
    ) {
        return MaterialPipelineDesc{
            .vertexShader =
                {.path = "Engine/Shader/X.slang", .entryPoint = "vs_main"},
            .fragmentShader =
                {.path = "Engine/Shader/X.slang", .entryPoint = fragmentEntry},
            .maskShader =
                {.path = "Engine/Shader/X.slang",
                 .entryPoint = "fs_masked_depth"},
            .domain = domain,
            .profile = "sm_6_8"
        };
    }

    PassPipelineDesc ColorPass() {
        return PassPipelineDesc{.renderTargetFormats = ColorFormats};
    }

    // a list over a scene whose primitives each draw one submesh
    class Fixture {
    public:
        FakeDevice device;
        PipelineCache cache{device};
        RenderScene scene;
        DrawList list{device, 8};

        MaterialHandle AddMaterial(
            CStr fragmentEntry,
            MaterialDomain domain = MaterialDomain::Opaque
        ) {
            return scene.Materials().Add(
                MaterialResource{.pipeline = Pipeline(fragmentEntry, domain)}
            );
        }

        // returns the primitive's row
        u32 AddPrimitive(MaterialHandle material, i32 baseVertex = 0) {
            const auto mesh = scene.Meshes().Add(
                MeshResource{
                    .subMeshes = {SubMesh{
                        .geometry = {
                            .firstIndex = 6,
                            .indexCount = 3,
                            .baseVertex = baseVertex
                        }
                    }},
                    .materials = {material}
                }
            );
            scene.Primitives().Add(PrimitiveSnapshot{.mesh = mesh});

            return static_cast<u32>(scene.Primitives().Count() - 1);
        }

        // the primitive's only submesh, as a cull would have kept it
        VisibleDraw Draw(
            u32 primitive,
            f32 depth,
            PrimitiveFlags flags =
                combine(PrimitiveFlags::Visible, PrimitiveFlags::CastShadow)
        ) const {
            const auto& primitives = scene.Primitives();
            const auto& mesh =
                scene.Meshes().GetRef(primitives.At(primitive).mesh);
            const auto identity = primitives.HandleAt(primitive).GetIndex();
            const auto material = scene.Materials().IndexOf(mesh.materials[0]);

            return VisibleDraw{
                .geometry = mesh.subMeshes[0].geometry,
                .primitive = primitive,
                .identity = static_cast<u32>(identity),
                .materialIndex = static_cast<u32>(material),
                .depth = depth,
                .flags = flags
            };
        }

        void Build(
            const VisibleSet& visible,
            const DrawFilter& filter,
            DrawOrder order
        ) {
            list.Build(scene, visible, cache, ColorPass(), filter, order);
        }

        // the list's rows as the GPU would read them
        Rows UploadedRows() {
            list.Upload();
            const auto push = list.Push(ScenePush{});

            Rows rows;
            for(u32 i = 0; i < list.DrawCount(); ++i) {
                rows.push_back(
                    device.transient.Read<DrawData>(
                        (push.drawBase + i) * static_cast<u32>(sizeof(DrawData))
                    )
                );
            }

            return rows;
        }

        std::vector<u32> DrawnObjects() {
            std::vector<u32> objects;
            for(const auto& row: UploadedRows())
                objects.push_back(row.objectID);

            return objects;
        }
    };
}

TEST(DrawList, OrderedKeepsFloatOrder) {
    constexpr auto Infinity = std::numeric_limits<f32>::infinity();
    constexpr std::array values = {
        -Infinity, -2.5f, -1e-30f, 0.0f, 1e-30f, 0.5f, 2.5f, Infinity
    };

    for(usize i = 1; i < values.size(); ++i) {
        EXPECT_LT(ordered(values[i - 1]), ordered(values[i]))
            << values[i - 1] << " before " << values[i];
    }
}

// a shadow list admits opaque casters only; the domain mask can hold several
TEST(DrawList, FilterAdmitsDomainsAndRequiredFlags) {
    Fixture f;
    const auto opaque = f.AddMaterial("fs_opaque");
    const auto glass = f.AddMaterial("fs_glass", MaterialDomain::Translucent);
    const auto caster = f.AddPrimitive(opaque);
    const auto receiver = f.AddPrimitive(opaque);
    const auto pane = f.AddPrimitive(glass);
    const VisibleSet visible{
        .draws = {
            f.Draw(caster, 1.0f),
            f.Draw(receiver, 2.0f, PrimitiveFlags::Visible),
            f.Draw(pane, 3.0f)
        }
    };

    f.Build(
        visible,
        DrawFilter{
            .domains = MaterialDomain::Opaque,
            .required = PrimitiveFlags::CastShadow
        },
        DrawOrder::PipelineThenNearFirst
    );
    EXPECT_EQ(f.DrawnObjects(), std::vector<u32>{caster});

    f.Build(
        visible,
        DrawFilter{.domains = MaterialDomain::Translucent},
        DrawOrder::FarFirst
    );
    EXPECT_EQ(f.DrawnObjects(), std::vector<u32>{pane});

    f.Build(
        visible,
        DrawFilter{
            .domains =
                combine(MaterialDomain::Opaque, MaterialDomain::Translucent)
        },
        DrawOrder::FarFirst
    );
    EXPECT_EQ(f.list.DrawCount(), 3u);
}

// what writes depth admits Masked beside Opaque, in a run of its own
TEST(DrawList, ACombinedFilterAdmitsMasked) {
    Fixture f;
    const auto opaque = f.AddMaterial("fs_opaque");
    const auto cutout = f.AddMaterial("fs_masked", MaterialDomain::Masked);
    const auto glass = f.AddMaterial("fs_glass", MaterialDomain::Translucent);
    const auto wall = f.AddPrimitive(opaque);
    const auto sign = f.AddPrimitive(cutout);
    const auto pane = f.AddPrimitive(glass);
    const VisibleSet visible{
        .draws = {f.Draw(wall, 1.0f), f.Draw(sign, 2.0f), f.Draw(pane, 3.0f)}
    };

    f.Build(
        visible,
        DrawFilter{
            .domains = combine(MaterialDomain::Opaque, MaterialDomain::Masked)
        },
        DrawOrder::PipelineThenNearFirst
    );
    EXPECT_EQ(f.DrawnObjects(), (std::vector<u32>{wall, sign}));
    EXPECT_EQ(f.list.RunCount(), 2u);

    f.Build(
        visible,
        DrawFilter{.domains = MaterialDomain::Opaque},
        DrawOrder::PipelineThenNearFirst
    );
    EXPECT_EQ(f.DrawnObjects(), std::vector<u32>{wall});
}

// pipelines in material-row order, then near first inside each
TEST(DrawList, OpaqueRunsPipelineFirstThenNearFirst) {
    Fixture f;
    const auto a = f.AddMaterial("fs_a");
    const auto b = f.AddMaterial("fs_b");
    const auto nearB = f.AddPrimitive(b);
    const auto farA = f.AddPrimitive(a);
    const auto nearA = f.AddPrimitive(a);
    const auto farB = f.AddPrimitive(b);
    const VisibleSet visible{
        .draws = {
            f.Draw(nearB, 1.0f),
            f.Draw(farA, 4.0f),
            f.Draw(nearA, 2.0f),
            f.Draw(farB, 3.0f)
        }
    };

    f.Build(
        visible,
        DrawFilter{.domains = MaterialDomain::Opaque},
        DrawOrder::PipelineThenNearFirst
    );

    EXPECT_EQ(f.DrawnObjects(), (std::vector<u32>{nearA, farA, nearB, farB}));
    EXPECT_EQ(f.list.RunCount(), 2u);
}

TEST(DrawList, TranslucentDrawsFarFirst) {
    Fixture f;
    const auto glass = f.AddMaterial("fs_glass", MaterialDomain::Translucent);
    const auto nearPane = f.AddPrimitive(glass);
    const auto farPane = f.AddPrimitive(glass);
    const auto middlePane = f.AddPrimitive(glass);
    const VisibleSet visible{
        .draws = {
            f.Draw(nearPane, 1.0f),
            f.Draw(farPane, 3.0f),
            f.Draw(middlePane, 2.0f)
        }
    };

    f.Build(
        visible,
        DrawFilter{.domains = MaterialDomain::Translucent},
        DrawOrder::FarFirst
    );

    EXPECT_EQ(
        f.DrawnObjects(),
        (std::vector<u32>{farPane, middlePane, nearPane})
    );
    EXPECT_EQ(f.list.RunCount(), 1u);
}

// depth order beats pipeline order, so a run breaks where the pipeline changes
TEST(DrawList, AlternatingPipelinesMakeThreeRuns) {
    Fixture f;
    const auto a = f.AddMaterial("fs_a", MaterialDomain::Translucent);
    const auto b = f.AddMaterial("fs_b", MaterialDomain::Translucent);
    const auto farA = f.AddPrimitive(a);
    const auto middleB = f.AddPrimitive(b);
    const auto nearA = f.AddPrimitive(a);
    const VisibleSet visible{
        .draws = {
            f.Draw(nearA, 1.0f),
            f.Draw(middleB, 2.0f),
            f.Draw(farA, 3.0f)
        }
    };

    f.Build(
        visible,
        DrawFilter{.domains = MaterialDomain::Translucent},
        DrawOrder::FarFirst
    );
    f.list.Upload();
    RecordingCommandList cmdList;
    f.list.Submit(cmdList, RHIIndexBufferView{});

    EXPECT_EQ(f.list.RunCount(), 3u);
    ASSERT_EQ(cmdList.batches.size(), 3u);
    EXPECT_EQ(cmdList.batches[0].pso, cmdList.batches[2].pso);
    EXPECT_NE(cmdList.batches[0].pso, cmdList.batches[1].pso);
    for(const auto& batch: cmdList.batches)
        EXPECT_EQ(batch.drawCount, 1u);
}

// baseInstance counts from the list's own slice, which drawBase names
TEST(DrawList, RowsAndArgsAddressTheListSlice) {
    Fixture f;
    // something else in the ring first, so the list's slice starts past 0
    f.device.AllocateTransient(100, 16);
    const auto a = f.AddMaterial("fs_a");
    const auto b = f.AddMaterial("fs_b");
    f.AddPrimitive(a);
    const auto first = f.AddPrimitive(b, 40);
    const auto second = f.AddPrimitive(b, 90);
    const VisibleSet visible{
        .draws = {f.Draw(first, 1.0f), f.Draw(second, 2.0f)}
    };

    f.Build(
        visible,
        DrawFilter{.domains = MaterialDomain::Opaque},
        DrawOrder::PipelineThenNearFirst
    );
    f.list.Upload();
    const auto push = f.list.Push(
        ScenePush{.materials = 7, .vertices = 9, .materialBase = 5}
    );
    RecordingCommandList cmdList;
    f.list.Submit(cmdList, RHIIndexBufferView{});

    EXPECT_EQ(push.draws, FakeBuffer::ReadableID);
    EXPECT_NE(push.drawBase, 0u);
    // the frame's fields pass through
    EXPECT_EQ(push.materials, 7u);
    EXPECT_EQ(push.vertices, 9u);
    EXPECT_EQ(push.materialBase, 5u);

    ASSERT_EQ(cmdList.batches.size(), 1u);
    const auto& batch = cmdList.batches[0];
    EXPECT_EQ(batch.args, &f.device.transient);
    EXPECT_EQ(batch.drawCount, 2u);

    constexpr auto RowSize = static_cast<u32>(sizeof(DrawData));
    constexpr auto ArgsSize = static_cast<u32>(sizeof(RHIDrawIndexedArgs));
    const std::array objects = {first, second};
    const std::array baseVertices = {40u, 90u};
    for(u32 i = 0; i < 2; ++i) {
        const auto row =
            f.device.transient.Read<DrawData>((push.drawBase + i) * RowSize);
        const auto args = f.device.transient.Read<RHIDrawIndexedArgs>(
            static_cast<u32>(batch.argsOffset) + i * ArgsSize
        );

        EXPECT_EQ(row.objectID, objects[i]);
        EXPECT_EQ(row.materialIndex, 1u);
        EXPECT_EQ(row.vbIndex, baseVertices[i]);
        EXPECT_EQ(args.baseInstance, i);
        EXPECT_EQ(args.baseVertex, 0);
        EXPECT_EQ(args.firstIndex, 6u);
        EXPECT_EQ(args.indexCount, 3u);
    }
}

// Equal depths fall back to the handle slot, which a Remove never moves,
// while the unrelated removal swaps the last row to the front.
TEST(DrawList, TranslucentOrderSurvivesARemove) {
    FakeDevice device;
    SceneRenderer renderer(device, SceneRendererDesc{});
    RenderScene scene;
    DrawList list(device, 8);
    // clip space is world space: x and y within 1, z within 0 and 1
    renderer.View(0).viewProj = unitMat();

    const auto addPrimitive = [&scene](MaterialDomain domain) {
        const auto material = scene.Materials().Add(
            MaterialResource{.pipeline = Pipeline("fs_main", domain)}
        );
        const auto mesh = scene.Meshes().Add(
            MeshResource{
                .subMeshes = {SubMesh{.geometry = {.indexCount = 3}}},
                .materials = {material}
            }
        );

        return scene.Primitives().Add(
            PrimitiveSnapshot{
                .worldBounds =
                    AABB3D{
                        .center = {0.0f, 0.0f, 0.5f},
                        .halfScale = 0.1f * ones()
                    },
                .mesh = mesh
            }
        );
    };
    const auto unrelated = addPrimitive(MaterialDomain::Opaque);
    for(u32 i = 0; i < 3; ++i)
        addPrimitive(MaterialDomain::Translucent);

    const auto drawnMaterials = [&] {
        renderer.BeginFrame(scene);
        list.Build(
            scene,
            renderer.Visible(0),
            renderer.Pipelines(),
            ColorPass(),
            DrawFilter{.domains = MaterialDomain::Translucent},
            DrawOrder::FarFirst
        );
        list.Upload();
        const auto push = list.Push(ScenePush{});

        std::vector<u32> materials;
        for(u32 i = 0; i < list.DrawCount(); ++i) {
            materials.push_back(
                device.transient
                    .Read<DrawData>(
                        (push.drawBase + i) * static_cast<u32>(sizeof(DrawData))
                    )
                    .materialIndex
            );
        }

        return materials;
    };

    const auto before = drawnMaterials();
    scene.Primitives().Remove(unrelated);

    EXPECT_EQ(before, (std::vector<u32>{1, 2, 3}));
    EXPECT_EQ(drawnMaterials(), before);
}

TEST(DrawList, AnEmptyListAllocatesNothing) {
    Fixture f;
    f.AddMaterial("fs_a");
    const auto allocations = f.device.transientAllocations;

    f.Build(
        VisibleSet{},
        DrawFilter{.domains = MaterialDomain::Opaque},
        DrawOrder::PipelineThenNearFirst
    );
    f.list.Upload();
    RecordingCommandList cmdList;
    f.list.Submit(cmdList, RHIIndexBufferView{});

    EXPECT_EQ(f.device.transientAllocations, allocations);
    EXPECT_EQ(f.list.DrawCount(), 0u);
    EXPECT_EQ(f.list.RunCount(), 0u);
    EXPECT_EQ(f.list.Push(ScenePush{}).draws, 0u);
    EXPECT_TRUE(cmdList.batches.empty());
}

// pipelines compile on the first frame even for what that frame cannot see
TEST(DrawList, AdmittedMaterialsResolveWithNothingVisible) {
    Fixture f;
    f.AddMaterial("fs_a");
    f.AddMaterial("fs_b");
    f.AddMaterial("fs_glass", MaterialDomain::Translucent);

    f.Build(
        VisibleSet{},
        DrawFilter{.domains = MaterialDomain::Opaque},
        DrawOrder::PipelineThenNearFirst
    );

    EXPECT_EQ(f.cache.Count(), 2u);
    EXPECT_EQ(f.device.creates, 2u);
}

// every list of a frame reads one cull per view, and the next frame culls anew
TEST(SceneRenderer, CullsOncePerViewPerFrame) {
    FakeDevice device;
    SceneRenderer renderer(device, SceneRendererDesc{.viewCount = 2});
    RenderScene scene;
    // clip space is world space for view 0; view 1 looks far to the side
    renderer.View(0).viewProj = unitMat();
    renderer.View(1).viewProj = translateMat({10.0f, 0.0f, 0.0f});

    const auto material =
        scene.Materials().Add(MaterialResource{.pipeline = Pipeline("fs_a")});
    const auto mesh = scene.Meshes().Add(
        MeshResource{
            .subMeshes = {SubMesh{.geometry = {.indexCount = 3}}},
            .materials = {material}
        }
    );
    const auto primitive = scene.Primitives().Add(
        PrimitiveSnapshot{
            .worldBounds =
                AABB3D{
                    .center = {0.0f, 0.0f, 0.5f},
                    .halfScale = 0.1f * ones()
                },
            .mesh = mesh
        }
    );

    renderer.BeginFrame(scene);
    const auto& culled = renderer.Visible(0);
    ASSERT_EQ(culled.primitiveCount, 1u);
    ASSERT_EQ(culled.draws.size(), 1u);
    EXPECT_EQ(culled.draws[0].depth, 0.5f);
    EXPECT_EQ(culled.draws[0].identity, primitive.GetIndex());

    // hidden after the cull: the frame keeps what it culled
    scene.Primitives().GetRef(primitive).flags = PrimitiveFlags::None;
    EXPECT_EQ(&renderer.Visible(0), &culled);
    EXPECT_EQ(renderer.Visible(0).primitiveCount, 1u);
    EXPECT_EQ(renderer.Visible(1).primitiveCount, 0u);

    renderer.BeginFrame(scene);
    EXPECT_EQ(renderer.Visible(0).primitiveCount, 0u);
}
