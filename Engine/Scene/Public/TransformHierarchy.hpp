#pragma once

#include <vector>
#include "GenericHandle.hpp"
#include "HandleTable.hpp"
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
        Transform local = Transform::Identity();

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
    class TransformHierarchy{
    private:
        // preorder
        std::vector<TransformNode> nodes;
        TransformNodeTable slots;

    private:
        auto& nodeOf(this auto& self, TransformHandle handle) noexcept{
            return self.nodes[self.slots.IndexOf(handle).value];
        }
        auto& localTransform(this auto& self, TransformHandle handle) noexcept{
            return self.nodeOf(handle).local;
        }

    public:
        TransformHierarchy() = default;
        ~TransformHierarchy() = default;
        CROWY_DECLARE_TRANSFERABLE(TransformHierarchy)

        // lifetime
        TransformHandle CreateNode(const Transform& local = Transform::Identity());
        // Asserts on a node that still has children
        void DestroyNode(TransformHandle handle);

        bool IsValid(TransformHandle handle) const noexcept{
            return slots.IsValid(handle);
        }

        Transform GetLocalTransform(TransformHandle handle) noexcept{
            return localTransform(handle);
        }
        void SetLocalTransform(TransformHandle handle, const Transform& transform) noexcept{
            localTransform(handle) = transform;
        }

        Vec3 GetLocalPosition(TransformHandle handle) noexcept{
            return localTransform(handle).position;
        }
        Vec4 GetLocalRotation(TransformHandle handle){
            return localTransform(handle).rotation;
        }
        Vec3 GetLocalScale(TransformHandle handle){
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

        usize Size() const noexcept{
            return nodes.size();
        }
        bool IsEmpty() const noexcept{
            return Size() == 0;
        }

    private:
        // The only place allowed to write parentIndex and slot bindings.
        void rebuildDerived() noexcept;
    };
}
