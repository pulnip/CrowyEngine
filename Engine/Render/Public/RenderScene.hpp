#pragma once

#include <vector>

#include "Geometry/Overlap3D.hpp"
#include "GeometryPool.hpp"
#include "LinearAlgebra.hpp"
#include "PackedTable.hpp"
#include "Primitives.hpp"
#include "RenderMaterial.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    struct MeshResource;
    struct PrimitiveSnapshot;

    using MeshHandle = GenericHandle<MeshResource>;
    using MeshTable = PackedTable<MeshResource>;
    using PrimitiveHandle = GenericHandle<PrimitiveSnapshot>;
    using PrimitiveTable = PackedTable<PrimitiveSnapshot>;

    enum class PrimitiveFlags : u32 {
        None = 0,
        Visible = 1u << 0,
        CastShadow = 1u << 1,
    };

    // One draw's worth of geometry, plus which of the mesh's materials it uses.
    struct SubMesh {
        GeometryAllocation geometry{};
        AABB3D localBounds{};
        // index into MeshResource::materials, not a row in the material table
        u32 materialSlot = 0;
    };

    using SubMeshes = std::vector<SubMesh>;
    using MaterialHandles = std::vector<MaterialHandle>;

    struct MeshResource {
        SubMeshes subMeshes;
        // one per material slot the submeshes name
        MaterialHandles materials;
        AABB3D localBounds{};
    };

    // What extraction writes and the only thing the renderer reads.
    // Note. View-independent
    struct PrimitiveSnapshot {
        Mat4 localToWorld = unitMat();
        // local bounds pushed through localToWorld at extract time
        AABB3D worldBounds{};
        MeshHandle mesh;
        PrimitiveFlags flags = PrimitiveFlags::Visible;
    };

    // The renderer's copy of the world:
    // persistent, written only by extraction.
    class RenderScene {
    private:
        MaterialTable materials;
        MeshTable meshes;
        PrimitiveTable primitives;

    public:
        RenderScene() = default;
        ~RenderScene() = default;
        CROWY_DECLARE_TRANSFERABLE(RenderScene)

        auto& Materials(this auto& self) noexcept { return self.materials; }
        auto& Meshes(this auto& self) noexcept { return self.meshes; }
        auto& Primitives(this auto& self) noexcept { return self.primitives; }

        void Clear() noexcept {
            primitives.Clear();
            meshes.Clear();
            materials.Clear();
        }
    };
}
