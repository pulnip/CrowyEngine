#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Assert.hpp"
#include "GenericHandle.hpp"
#include "HandleTable.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    struct TransformNode;

    // The index in this handle is a slot number, not a node array position
    using TransformHandle = GenericHandle<TransformNode>;
    using TransformNodeTable = HandleTable<TransformNode>;
    using TransformSlot = TransformNodeTable::Slot;
    using TransformIndex = TransformNodeTable::Index;

    struct TransformNode{
        // this node or an ancestor scales unevenly
        static constexpr u32 NON_UNIFORM_IN_CHAIN = 1u << 0;
        // already warned about, so the log stays readable
        static constexpr u32 WARNED_NON_UNIFORM = 1u << 1;

        Transform local = Transform::Identity();
        // caches, refreshed by UpdateWorldTransforms
        Mat4 world = unitMat();
        // composed along the chain instead of read back out of world, which would
        // divide by a zero scale and read a mirrored basis as a rotation
        Vec4 worldRotation = unitQuat();
        u32 flags = 0;

        // reverse mapping
        TransformSlot slot = TransformSlot::Invalid();
        // source of truth for the parent link, stays valid while nodes move
        TransformSlot parentSlot = TransformSlot::Invalid();
        // derived from parentSlot
        TransformIndex parentIndex = TransformIndex::Invalid();
        // subtree node count, including this node
        usize subtreeSize = 1;

        bool IsRoot() const noexcept{
            return !parentSlot.IsValid();
        }
    };

    // Nodes are kept in one preorder array,
    // so a parent sits in front of its children
    // and its subtree occupies a contiguous range.
    //
    // Structural changes land on CommitStructuralChanges(),
    // not on the call that asks for them.
    // Handles and local transforms do not wait for it:
    //   a handle is valid the moment CreateNode() returns,
    //   and its local transform reads and writes right away.
    // Everything else answers as of the last commit.
    //
    // Not thread safe, const methods included: the inverse cache is filled in on read.
    class TransformHierarchy{
    private:
        // A slot holding a handle to a node that is not in the array yet. Never a
        // position, but valid, so the handle it was issued for stays valid too.
        static constexpr TransformIndex PENDING_INDEX{TransformIndex::INVALID - 1};

        struct PendingCreate{
            TransformSlot slot;
            TransformSlot parentSlot;
        };
        // Slots, not indexes: every command before this one moves nodes around,
        // so the position has to be looked up when the turn comes.
        struct PendingReparent{
            TransformSlot slot;
            TransformSlot parentSlot;
        };

        // preorder
        std::vector<TransformNode> nodes;
        TransformNodeTable slots;

        std::vector<PendingCreate> pendingCreates;
        std::vector<PendingReparent> pendingReparents;
        std::unordered_map<TransformSlot, Transform> pendingLocals;
        std::unordered_set<TransformSlot> pendingDestroys;

        // Kept out of TransformNode on purpose: another 64 bytes per node would
        // slow the sequential pass in UpdateWorldTransforms down for the few nodes
        // that ever get asked for an inverse.
        mutable std::vector<Mat4> worldInverses;
        mutable std::vector<u8> inverseValid;

    private:
        TransformIndex indexOf(TransformHandle handle) const noexcept{
            CROWY_ASSERT(IsValid(handle));

            return slots.IndexOf(handle);
        }
        auto& nodeOf(this auto& self, TransformHandle handle) noexcept{
            auto index = self.indexOf(handle);
            CROWY_ASSERT(index != PENDING_INDEX);

            return self.nodes[index.value];
        }
        auto& localTransform(this auto& self, TransformHandle handle) noexcept{
            auto index = self.indexOf(handle);

            return index == PENDING_INDEX ?
                self.pendingLocals.at(TransformNodeTable::SlotOf(handle)) :
                self.nodes[index.value].local;
        }

    public:
        TransformHierarchy() = default;
        ~TransformHierarchy() = default;
        CROWY_DECLARE_TRANSFERABLE(TransformHierarchy)

        // lifetime
        TransformHandle CreateNode(const Transform& local = Transform::Identity());
        TransformHandle CreateNode(
            TransformHandle parent,
            const Transform& local = Transform::Identity()
        );
        // Asserts unless every descendant is already destroyed, so a whole subtree
        // has to go leaf first. No handle but the given one is invalidated, and an
        // already invalid one is ignored.
        void DestroyNode(TransformHandle handle);
        // Notice! This invalidates handles the caller never named. Every one of
        // them lands in outDestroyed, and the caller is expected to reconcile
        // whatever it maps them from.
        void DestroySubtree(
            TransformHandle handle,
            std::vector<TransformHandle>& outDestroyed
        );

        // An invalid parent promotes to root. Asserts on a parent that sits inside
        // the moved subtree.
        void SetParent(TransformHandle handle, TransformHandle newParent);
        void PromoteToRoot(TransformHandle handle){
            SetParent(handle, TransformHandle::InvalidHandle());
        }

        void CommitStructuralChanges();
        bool HasPendingChanges() const noexcept{
            return !pendingCreates.empty() ||
                !pendingReparents.empty() ||
                !pendingDestroys.empty();
        }

        // hierarchy, as of the last commit
        TransformHandle GetParent(TransformHandle handle) const noexcept;
        usize GetChildCount(TransformHandle handle) const noexcept;

        // A destroyed node keeps its place in the array until the commit, but its
        // handle dies right away, the same way a created one lives right away.
        bool IsValid(TransformHandle handle) const noexcept{
            return slots.IsValid(handle) &&
                !pendingDestroys.contains(TransformNodeTable::SlotOf(handle));
        }

        Transform GetLocalTransform(TransformHandle handle) const noexcept{
            return localTransform(handle);
        }
        void SetLocalTransform(TransformHandle handle, const Transform& transform) noexcept{
            localTransform(handle) = transform;
        }

        Vec3 GetLocalPosition(TransformHandle handle) const noexcept{
            return localTransform(handle).position;
        }
        Vec4 GetLocalRotation(TransformHandle handle) const noexcept{
            return localTransform(handle).rotation;
        }
        Vec3 GetLocalScale(TransformHandle handle) const noexcept{
            return localTransform(handle).scale;
        }
        void SetLocalPosition(TransformHandle handle, const Vec3& position) noexcept{
            localTransform(handle).position = position;
        }
        void SetLocalRotation(TransformHandle handle, const Vec4& rotation) noexcept{
            localTransform(handle).rotation = rotation;
        }
        void SetLocalScale(TransformHandle handle, const Vec3& scale) noexcept{
            localTransform(handle).scale = scale;
        }

        // world transform
        void UpdateWorldTransforms();

        const Mat4& GetWorldMatrix(TransformHandle handle) const noexcept{
            return nodeOf(handle).world;
        }
        // Walks the ancestor chain instead of reading the cache, and leaves the
        // cache alone
        Mat4 ComputeWorldMatrixNow(TransformHandle handle) const noexcept;

        Vec3 GetWorldPosition(TransformHandle handle) const noexcept{
            return static_cast<Vec3>(nodeOf(handle).world[3]);
        }
        // The rotation of the TRS chain. Where the chain scales unevenly this is
        // no longer the rotation of GetWorldMatrix, which carries shear as well;
        // UpdateWorldTransforms warns about those nodes once.
        Vec4 GetWorldRotation(TransformHandle handle) const noexcept{
            return nodeOf(handle).worldRotation;
        }
        const Mat4& GetWorldInverse(TransformHandle handle) const noexcept;

        // committed nodes only
        usize Size() const noexcept{
            return nodes.size();
        }
        bool IsEmpty() const noexcept{
            return Size() == 0;
        }

    private:
        void commitDestroys();
        void commitCreates();
        void insertPendingNode(const PendingCreate& create);
        void commitReparents();
        void reparent(TransformSlot slot, TransformSlot newParentSlot);

        bool everyDescendantIsDestroyed(TransformIndex index) const noexcept;
        bool isInSubtree(TransformIndex root, TransformIndex candidate) const noexcept;
        TransformIndex insertionPointOf(TransformSlot parentSlot) const noexcept;
        void insertNode(TransformIndex at, TransformNode node);
        void moveBlock(TransformIndex from, usize size, TransformIndex to);
        // Keeps the slot table pointing at nodes that a shift moved
        void rebindFrom(TransformIndex from) noexcept;
        void growAncestors(TransformSlot parentSlot, usize count = 1) noexcept;
        void shrinkAncestors(TransformSlot parentSlot, usize count = 1) noexcept;

        // The only place allowed to write parentIndex
        void rebuildParentIndexes() noexcept;

        void invalidateInverses() const noexcept;
        void scheduleDestroy(TransformIndex index, TransformSlot slot);
        void cancelPendingCreate(TransformSlot slot);
        void cancelPendingCreatesUnder(
            std::unordered_set<TransformSlot>& doomed,
            std::vector<TransformHandle>& outCancelled
        );
        static void warnOnNonUniformChain(TransformNode& node);

        friend class TransformHierarchyAttorney;
    };
}
