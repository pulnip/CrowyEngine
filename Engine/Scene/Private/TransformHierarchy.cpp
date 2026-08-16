#include <algorithm>
#include <cmath>
#include <utility>
#include "TransformHierarchy.hpp"
#include "Assert.hpp"
#include "LogLocal.hpp"

namespace Crowy
{
    namespace{
        // f32 carries about 7 digits, and a scale that came back from a matrix
        // decomposition drifts a few of the last ones. Relative to the largest
        // component, so a large uniform scale is not reported either.
        constexpr f32 SCALE_EPSILON = 1e-5f;

        bool isUniform(Vec3 scale) noexcept{
            auto largest = std::max({
                std::abs(scale.x), std::abs(scale.y), std::abs(scale.z)
            });
            auto tolerance = SCALE_EPSILON * std::max(largest, 1.0f);

            return std::abs(scale.x - scale.y) <= tolerance &&
                std::abs(scale.y - scale.z) <= tolerance;
        }

        void setFlag(u32& flags, u32 bit, bool on) noexcept{
            flags = on ? (flags | bit) : (flags & ~bit);
        }
    }

    void TransformHierarchy::rebindFrom(TransformIndex from) noexcept{
        for(usize i=from.value; i<nodes.size(); ++i){
            slots.Bind(nodes[i].slot, TransformIndex{i});
        }
    }

    void TransformHierarchy::rebuildParentIndexes() noexcept{
        for(auto& node: nodes){
            node.parentIndex = node.IsRoot() ?
                TransformIndex::Invalid() :
                slots.IndexOf(node.parentSlot);
        }
    }

    // A subtree is contiguous, so its last node is also the last of the parent's
    // children. Later siblings land behind earlier ones, in creation order.
    TransformIndex TransformHierarchy::insertionPointOf(
        TransformSlot parentSlot
    ) const noexcept{
        if(!parentSlot.IsValid()){
            return TransformIndex{nodes.size()};
        }

        auto parentIndex = slots.IndexOf(parentSlot);

        return TransformIndex{
            parentIndex.value + nodes[parentIndex.value].subtreeSize
        };
    }

    void TransformHierarchy::insertNode(TransformIndex at, TransformNode node){
        nodes.insert(
            nodes.begin() + static_cast<isize>(at.value),
            std::move(node)
        );
        rebindFrom(at);
    }

    void TransformHierarchy::growAncestors(
        TransformSlot parentSlot, usize count
    ) noexcept{
        for(auto slot=parentSlot; slot.IsValid();){
            auto& ancestor = nodes[slots.IndexOf(slot).value];
            ancestor.subtreeSize += count;
            slot = ancestor.parentSlot;
        }
    }

    void TransformHierarchy::shrinkAncestors(
        TransformSlot parentSlot, usize count
    ) noexcept{
        for(auto slot=parentSlot; slot.IsValid();){
            auto& ancestor = nodes[slots.IndexOf(slot).value];
            ancestor.subtreeSize -= count;
            slot = ancestor.parentSlot;
        }
    }

    bool TransformHierarchy::isInSubtree(
        TransformIndex root, TransformIndex candidate
    ) const noexcept{
        // I2 makes this a range test instead of a walk
        return candidate.value >= root.value &&
            candidate.value < root.value + nodes[root.value].subtreeSize;
    }

    void TransformHierarchy::moveBlock(
        TransformIndex from, usize size, TransformIndex to
    ){
        auto begin = nodes.begin();
        auto first = static_cast<isize>(from.value);
        auto last = static_cast<isize>(from.value + size);
        auto target = static_cast<isize>(to.value);

        if(to.value > from.value){
            std::rotate(begin + first, begin + last, begin + target);
        }
        else{
            std::rotate(begin + target, begin + first, begin + last);
        }

        rebindFrom(TransformIndex{std::min(from.value, to.value)});
    }

    bool TransformHierarchy::everyDescendantIsDestroyed(
        TransformIndex index
    ) const noexcept{
        auto end = index.value + nodes[index.value].subtreeSize;

        for(usize i=index.value+1; i<end; ++i){
            if(!pendingDestroys.contains(nodes[i].slot)){
                return false;
            }
        }

        return true;
    }

    TransformHandle TransformHierarchy::CreateNode(const Transform& local){
        return CreateNode(TransformHandle::InvalidHandle(), local);
    }

    TransformHandle TransformHierarchy::CreateNode(
        TransformHandle parent, const Transform& local
    ){
        CROWY_ASSERT(!parent.IsValid() || IsValid(parent));

        auto handle = slots.Acquire(PENDING_INDEX);
        auto slot = TransformNodeTable::SlotOf(handle);

        pendingCreates.push_back(PendingCreate{
            .slot = slot,
            .parentSlot = parent.IsValid() ?
                TransformNodeTable::SlotOf(parent) :
                TransformSlot::Invalid()
        });
        pendingLocals.emplace(slot, local);

        return handle;
    }

    void TransformHierarchy::DestroyNode(TransformHandle handle){
        auto index = indexOf(handle);
        auto slot = TransformNodeTable::SlotOf(handle);

        CROWY_ASSERT(
            std::ranges::none_of(
                pendingCreates,
                [slot](const PendingCreate& create){
                    return create.parentSlot == slot;
                }
            ),
            "DestroyNode: an uncommitted child still names this node as its parent"
        );
        CROWY_ASSERT(
            std::ranges::none_of(
                pendingReparents,
                [slot](const PendingReparent& command){
                    return command.slot == slot || command.parentSlot == slot;
                }
            ),
            "DestroyNode: an uncommitted reparent still names this node"
        );

        if(index == PENDING_INDEX){
            // the node never reached the array, so create and destroy cancel out
            std::erase_if(
                pendingCreates,
                [slot](const PendingCreate& create){
                    return create.slot == slot;
                }
            );
            pendingLocals.erase(slot);
            slots.Release(handle);

            return;
        }

        CROWY_ASSERT(
            everyDescendantIsDestroyed(index),
            "DestroyNode: node (slot {}) still has a living descendant",
            slot.value
        );

        pendingDestroys.insert(slot);
    }

    void TransformHierarchy::SetParent(
        TransformHandle handle, TransformHandle newParent
    ){
        CROWY_ASSERT(IsValid(handle));
        CROWY_ASSERT(!newParent.IsValid() || IsValid(newParent));
        CROWY_ASSERT(handle != newParent, "SetParent: a node cannot be its own parent");

        auto slot = TransformNodeTable::SlotOf(handle);
        auto parentSlot = newParent.IsValid() ?
            TransformNodeTable::SlotOf(newParent) :
            TransformSlot::Invalid();

        // still only a plan, so rewriting the plan is the whole job
        for(auto& create: pendingCreates){
            if(create.slot == slot){
                create.parentSlot = parentSlot;

                return;
            }
        }

        pendingReparents.push_back(PendingReparent{
            .slot = slot,
            .parentSlot = parentSlot
        });
    }

    void TransformHierarchy::reparent(
        TransformSlot slot, TransformSlot newParentSlot
    ){
        auto index = slots.IndexOf(slot);
        auto& node = nodes[index.value];
        if(node.parentSlot == newParentSlot){
            return;
        }

        auto size = node.subtreeSize;
        CROWY_ASSERT(
            !newParentSlot.IsValid() ||
                !isInSubtree(index, slots.IndexOf(newParentSlot)),
            "SetParent: slot {} sits inside the subtree of slot {}",
            newParentSlot.value, slot.value
        );

        // Measured before anything moves. std::rotate lands the block exactly
        // where a destination read off the old array points, so the gap the block
        // leaves behind needs no correction of its own.
        auto destination = insertionPointOf(newParentSlot);

        auto oldParentSlot = node.parentSlot;
        // the block root carries the only link that changes
        node.parentSlot = newParentSlot;

        // after the destination is measured: growing first would push it past the
        // block by the block's own size
        shrinkAncestors(oldParentSlot, size);
        growAncestors(newParentSlot, size);

        moveBlock(index, size, destination);
    }

    void TransformHierarchy::commitReparents(){
        for(const auto& command: pendingReparents){
            reparent(command.slot, command.parentSlot);
        }
        pendingReparents.clear();
    }

    void TransformHierarchy::commitDestroys(){
        if(pendingDestroys.empty()){
            return;
        }

        // while every slot still points at its node
        for(auto slot: pendingDestroys){
            shrinkAncestors(nodes[slots.IndexOf(slot).value].parentSlot);
        }

        // erase_if keeps the relative order of the survivors, so preorder holds
        std::erase_if(
            nodes,
            [this](const TransformNode& node){
                return pendingDestroys.contains(node.slot);
            }
        );

        for(auto slot: pendingDestroys){
            slots.Release(slots.HandleOf(slot));
        }
        pendingDestroys.clear();

        rebindFrom(TransformIndex{0});
    }

    void TransformHierarchy::insertPendingNode(const PendingCreate& create){
        auto node = TransformNode{
            .local = pendingLocals.at(create.slot),
            .slot = create.slot,
            .parentSlot = create.parentSlot
        };
        pendingLocals.erase(create.slot);

        insertNode(
            insertionPointOf(create.parentSlot),
            std::move(node)
        );
        growAncestors(create.parentSlot);
    }

    void TransformHierarchy::commitCreates(){
        // A create can name another create from the same batch as its parent, and
        // SetParent can put that parent behind its own child in the list, so the
        // list drains over several passes instead of one.
        while(!pendingCreates.empty()){
            std::vector<PendingCreate> waiting;

            for(const auto& create: pendingCreates){
                auto parentIsWaiting = create.parentSlot.IsValid() &&
                    slots.IndexOf(create.parentSlot) == PENDING_INDEX;

                if(parentIsWaiting){
                    waiting.push_back(create);
                    continue;
                }

                insertPendingNode(create);
            }

            CROWY_ASSERT(
                waiting.size() < pendingCreates.size(),
                "CommitStructuralChanges: uncommitted nodes name each other as parent"
            );
            pendingCreates = std::move(waiting);
        }

        CROWY_ASSERT(pendingLocals.empty());
    }

    void TransformHierarchy::CommitStructuralChanges(){
        // Destroys first, so a create never has to make room next to a corpse.
        // Reparents last, so a node can be moved under a parent born in the very
        // same batch.
        commitDestroys();
        commitCreates();
        commitReparents();

        rebuildParentIndexes();
        // positions moved, so a cached inverse would answer for another node now
        invalidateInverses();
    }

    void TransformHierarchy::UpdateWorldTransforms() noexcept{
        // Parallel note: root subtrees are independent of each other, and by I2
        // each one is a contiguous range, so a fork-join over roots parallelizes
        // as is. One thread is enough for now, so it stays a plain loop.
        // (Commit is a different story: it memmoves and its phases are ordered.)
        for(auto& node: nodes){
            // I1 puts every parent in front, so its world is already this frame's
            auto local = modelMat(node.local);
            auto inherited = !node.IsRoot() &&
                (nodes[node.parentIndex.value].flags &
                    TransformNode::NON_UNIFORM_IN_CHAIN) != 0;

            node.world = node.IsRoot() ?
                local :
                nodes[node.parentIndex.value].world * local;

            setFlag(
                node.flags,
                TransformNode::NON_UNIFORM_IN_CHAIN,
                inherited || !isUniform(node.local.scale)
            );
        }

        invalidateInverses();
    }

    Mat4 TransformHierarchy::ComputeWorldMatrixNow(
        TransformHandle handle
    ) const noexcept{
        const auto& node = nodeOf(handle);
        auto world = modelMat(node.local);

        for(auto index=node.parentIndex; index.IsValid(); ){
            const auto& ancestor = nodes[index.value];
            world = modelMat(ancestor.local) * world;
            index = ancestor.parentIndex;
        }

        return world;
    }

    void TransformHierarchy::invalidateInverses() const noexcept{
        worldInverses.resize(nodes.size());
        inverseValid.assign(nodes.size(), 0);
    }

    void TransformHierarchy::warnOnNonUniformChain(const TransformNode& node) const{
        auto chainIsUneven = (node.flags & TransformNode::NON_UNIFORM_IN_CHAIN) != 0;
        auto alreadyWarned = (node.flags & TransformNode::WARNED_NON_UNIFORM) != 0;
        if(!chainIsUneven || alreadyWarned){
            return;
        }

        node.flags |= TransformNode::WARNED_NON_UNIFORM;
        LOG_WARN(
            "GetWorldRotation: non-uniform scale in the chain of slot {}, "
            "the rotation is an approximation",
            node.slot.value
        );
    }

    Vec3 TransformHierarchy::GetWorldPosition(
        TransformHandle handle
    ) const noexcept{
        return static_cast<Vec3>(nodeOf(handle).world[3]);
    }

    Vec4 TransformHierarchy::GetWorldRotation(
        TransformHandle handle
    ) const noexcept{
        const auto& node = nodeOf(handle);
        warnOnNonUniformChain(node);

        // the basis carries the scale, so it has to come off before the quaternion
        return quat(
            normalize(static_cast<Vec3>(node.world[0])),
            normalize(static_cast<Vec3>(node.world[1])),
            normalize(static_cast<Vec3>(node.world[2]))
        );
    }

    const Mat4& TransformHierarchy::GetWorldInverse(
        TransformHandle handle
    ) const noexcept{
        auto index = indexOf(handle);
        CROWY_ASSERT(index != PENDING_INDEX);
        CROWY_ASSERT(index.value < inverseValid.size());

        if(!inverseValid[index.value]){
            worldInverses[index.value] = inverseAffine(nodes[index.value].world);
            inverseValid[index.value] = 1;
        }

        return worldInverses[index.value];
    }

    TransformHandle TransformHierarchy::GetParent(
        TransformHandle handle
    ) const noexcept{
        if(indexOf(handle) == PENDING_INDEX){
            return TransformHandle::InvalidHandle();
        }

        auto parentSlot = nodeOf(handle).parentSlot;

        return parentSlot.IsValid() ?
            slots.HandleOf(parentSlot) :
            TransformHandle::InvalidHandle();
    }

    usize TransformHierarchy::GetChildCount(
        TransformHandle handle
    ) const noexcept{
        auto index = indexOf(handle);
        if(index == PENDING_INDEX){
            return 0;
        }

        auto end = index.value + nodes[index.value].subtreeSize;

        usize count = 0;
        // jumping a whole subtree at a time leaves only the direct children
        for(usize i=index.value+1; i<end; i+=nodes[i].subtreeSize){
            ++count;
        }

        return count;
    }
}
