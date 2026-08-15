#include <algorithm>
#include <utility>
#include "TransformHierarchy.hpp"
#include "Assert.hpp"

namespace Crowy
{
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

    void TransformHierarchy::eraseNode(TransformIndex at){
        // erase keeps the relative order of the survivors, so preorder holds
        nodes.erase(nodes.begin() + static_cast<isize>(at.value));
        rebindFrom(at);
    }

    void TransformHierarchy::growAncestors(TransformSlot parentSlot) noexcept{
        for(auto slot=parentSlot; slot.IsValid();){
            auto& ancestor = nodes[slots.IndexOf(slot).value];
            ++ancestor.subtreeSize;
            slot = ancestor.parentSlot;
        }
    }

    void TransformHierarchy::shrinkAncestors(TransformSlot parentSlot) noexcept{
        for(auto slot=parentSlot; slot.IsValid();){
            auto& ancestor = nodes[slots.IndexOf(slot).value];
            --ancestor.subtreeSize;
            slot = ancestor.parentSlot;
        }
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

        if(slots.IndexOf(handle) == PENDING_INDEX){
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

        auto index = slots.IndexOf(handle);
        auto& node = nodes[index.value];
        CROWY_ASSERT(
            node.subtreeSize == 1,
            "DestroyNode: node (slot {}) still has {} descendant(s)",
            slot.value, node.subtreeSize - 1
        );

        shrinkAncestors(node.parentSlot);
        eraseNode(index);
        slots.Release(handle);

        rebuildParentIndexes();
    }

    void TransformHierarchy::CommitStructuralChanges(){
        for(const auto& create: pendingCreates){
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
        pendingCreates.clear();
        CROWY_ASSERT(pendingLocals.empty());

        rebuildParentIndexes();
    }

    TransformHandle TransformHierarchy::GetParent(
        TransformHandle handle
    ) const noexcept{
        if(slots.IndexOf(handle) == PENDING_INDEX){
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
        if(slots.IndexOf(handle) == PENDING_INDEX){
            return 0;
        }

        auto index = slots.IndexOf(handle);
        auto end = index.value + nodes[index.value].subtreeSize;

        usize count = 0;
        // jumping a whole subtree at a time leaves only the direct children
        for(usize i=index.value+1; i<end; i+=nodes[i].subtreeSize){
            ++count;
        }

        return count;
    }
}
