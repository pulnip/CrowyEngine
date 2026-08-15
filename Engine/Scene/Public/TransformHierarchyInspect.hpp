#pragma once

// Test and debug only. Production code must not include this.

#include <format>
#include <span>
#include "TransformHierarchy.hpp"

namespace Crowy
{
    class TransformHierarchyAttorney{
    public:
        static std::span<const TransformNode> Nodes(
            const TransformHierarchy& hierarchy
        ) noexcept{
            return hierarchy.nodes;
        }
        static const TransformNodeTable& Slots(
            const TransformHierarchy& hierarchy
        ) noexcept{
            return hierarchy.slots;
        }
    };

    struct HierarchyView{
        std::span<const TransformNode> nodes;
        const TransformNodeTable* slots = nullptr;
    };

    inline HierarchyView MakeView(const TransformHierarchy& hierarchy) noexcept{
        return HierarchyView{
            .nodes = TransformHierarchyAttorney::Nodes(hierarchy),
            .slots = &TransformHierarchyAttorney::Slots(hierarchy)
        };
    }

    // Knows nothing about TransformHierarchy, so a hand-broken view can be fed in
    // to check that the checker itself still bites.
    inline bool CheckInvariants(const HierarchyView& view, Str* outError = nullptr){
        const auto& nodes = view.nodes;
        const auto count = nodes.size();

        auto fail = [outError](Str message){
            if(outError){
                *outError = std::move(message);
            }

            return false;
        };

        if(!view.slots){
            return fail("view has no slot table");
        }

        for(usize i=0; i<count; ++i){
            if(nodes[i].subtreeSize < 1){
                return fail(std::format("[{}] subtreeSize is 0", i));
            }
        }

        // I3. handles round trip both ways
        for(usize i=0; i<count; ++i){
            const auto& node = nodes[i];

            if(!view.slots->IsLiving(node.slot)){
                return fail(std::format(
                    "I3 [{}] slot {} is not living",
                    i, node.slot.value
                ));
            }
            if(view.slots->IndexOf(node.slot).value != i){
                return fail(std::format(
                    "I3 [{}] slot {} points at index {}",
                    i, node.slot.value, view.slots->IndexOf(node.slot).value
                ));
            }

            const auto expected = node.IsRoot() ?
                TransformIndex::Invalid() :
                view.slots->IndexOf(node.parentSlot);
            if(node.parentIndex != expected){
                return fail(std::format(
                    "I3 [{}] parentIndex {} disagrees with parentSlot {}",
                    i, node.parentIndex.value, node.parentSlot.value
                ));
            }
            if(!node.IsRoot() && !view.slots->IsLiving(node.parentSlot)){
                return fail(std::format(
                    "I3 [{}] parentSlot is not living",
                    i
                ));
            }
        }

        // I1. a parent always sits in front of its children
        for(usize i=0; i<count; ++i){
            const auto& node = nodes[i];

            if(node.IsRoot() != !node.parentIndex.IsValid()){
                return fail(std::format("I1 [{}] root-ness disagrees between slot and index", i));
            }
            if(!node.IsRoot() && node.parentIndex.value >= i){
                return fail(std::format(
                    "I1 [{}] parent sits at {}, not in front", i, node.parentIndex.value
                ));
            }
        }

        // I2. a subtree fills exactly [i, i + subtreeSize)
        for(usize i=0; i<count; ++i){
            const auto end = i + nodes[i].subtreeSize;
            if(end > count){
                return fail(std::format("I2 [{}] subtree runs past the end at {}", i, end));
            }

            usize covered = 1;
            for(usize j=i+1; j<end; j+=nodes[j].subtreeSize){
                if(nodes[j].parentIndex.value != i){
                    return fail(std::format(
                        "I2 [{}] node {} inside the range is not a direct child", i, j
                    ));
                }
                covered += nodes[j].subtreeSize;
            }
            if(covered != nodes[i].subtreeSize){
                return fail(std::format(
                    "I2 [{}] children cover {} nodes, subtreeSize says {}",
                    i, covered, nodes[i].subtreeSize
                ));
            }
        }

        // I4. the roots together cover everything, exactly once
        usize rooted = 0;
        for(const auto& node: nodes){
            if(node.IsRoot()){
                rooted += node.subtreeSize;
            }
        }
        if(rooted != count){
            return fail(std::format(
                "I4 roots cover {} nodes out of {}", rooted, count
            ));
        }

        return true;
    }

    inline bool CheckInvariants(const TransformHierarchy& hierarchy, Str* outError = nullptr){
        return CheckInvariants(MakeView(hierarchy), outError);
    }
}
