#include "TransformHierarchy.hpp"
#include "Assert.hpp"

namespace Crowy
{
    void TransformHierarchy::rebuildDerived() noexcept{
        for(usize i=0; i<nodes.size(); ++i){
            slots.Bind(
                nodes[i].slot,
                TransformIndex{i}
            );
        }

        for(auto& node: nodes){
            node.parentIndex = node.IsRoot() ?
                TransformIndex::Invalid() :
                slots.IndexOf(node.parentSlot);
        }
    }

    TransformHandle TransformHierarchy::CreateNode(const Transform& local){
        auto handle = slots.Acquire(TransformIndex{nodes.size()});

        nodes.push_back(TransformNode{
            .local = local,
            .slot = TransformNodeTable::SlotOf(handle)
        });

        return handle;
    }

    void TransformHierarchy::DestroyNode(TransformHandle handle){
        auto index = slots.IndexOf(handle);
        CROWY_ASSERT(
            nodes[index.value].subtreeSize == 1,
            "DestroyNode: node (slot {}) still has {} descendant(s)",
            handle.GetIndex(), nodes[index.value].subtreeSize - 1
        );

        // erase keeps the relative order of the survivors, so preorder holds
        nodes.erase(nodes.begin() + static_cast<isize>(index.value));
        slots.Release(handle);

        rebuildDerived();
    }
}
