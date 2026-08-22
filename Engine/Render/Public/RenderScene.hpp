#pragma once

#include <span>
#include <vector>

#include "GenericHandle.hpp"
#include "Geometry/Overlap3D.hpp"
#include "GeometryPool.hpp"
#include "HandleTable.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    struct PrimitiveSnapshot;
    struct PrimitiveTag;

    using PrimitiveHandle = GenericHandle<PrimitiveTag>;
    using PrimitiveTable = HandleTable<PrimitiveTag>;
    using PrimitiveSlots = std::vector<PrimitiveTable::Slot>;
    using PrimitiveSnapshots = std::vector<PrimitiveSnapshot>;

    enum class PrimitiveFlags : u32 {
        None = 0,
        Visible = 1u << 0,
        CastShadow = 1u << 1,
    };

    // Note. View-independent
    struct PrimitiveSnapshot {
        Mat4 localToWorld = unitMat();
        // local bounds pushed through localToWorld at extract time
        AABB3D worldBounds{};
        GeometryAllocation geometry{};
        PrimitiveFlags flags = PrimitiveFlags::Visible;
    };

    // The renderer's copy of the world:
    // persistent, written only by extraction.
    class RenderScene {
    private:
        PrimitiveTable slots;
        PrimitiveSnapshots primitives;
        // HandleTable does not store, needed by swap-remove.
        // Kept out of PrimitiveSnapshot so the row stays what extraction writes.
        PrimitiveSlots slotOfRow;

    public:
        RenderScene() = default;
        ~RenderScene() = default;
        CROWY_DECLARE_TRANSFERABLE(RenderScene)

        PrimitiveHandle Add(const PrimitiveSnapshot& snapshot);
        void Write(PrimitiveHandle handle, const PrimitiveSnapshot& snapshot);
        void Remove(PrimitiveHandle handle);
        // expires every handle ever issued
        void Clear() noexcept;

        bool IsValid(PrimitiveHandle handle) const noexcept {
            return slots.IsValid(handle);
        }

        const PrimitiveSnapshot& Read(PrimitiveHandle handle) const noexcept {
            CROWY_ASSERT(IsValid(handle));

            return primitives[slots.IndexOf(handle).value];
        }

        usize PrimitiveCount() const noexcept { return primitives.size(); }
        const PrimitiveSnapshot& PrimitiveAt(usize index) const noexcept {
            CROWY_ASSERT(index < primitives.size());

            return primitives[index];
        }
        std::span<const PrimitiveSnapshot> Primitives() const noexcept {
            return primitives;
        }
    };
}
