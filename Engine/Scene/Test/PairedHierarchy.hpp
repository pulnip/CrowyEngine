#pragma once

// Test only. Feeds one operation sequence to both implementations and holds them
// against each other.

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <vector>
#include <gtest/gtest.h>
#include "ReferenceHierarchy.hpp"
#include "TransformHierarchyInspect.hpp"

namespace Crowy
{
    class PairedHierarchy{
    public:
        static constexpr usize NO_PARENT = std::numeric_limits<usize>::max();
        // Both sides evaluate the same products in the same order, so they are
        // expected to agree to the bit. This is slack for the day they stop
        // associating the same way, on values that stay near 1.
        static constexpr f32 EPSILON = 1e-4f;

    private:
        struct Pair{
            TransformHandle handle;
            RefNode* node = nullptr;
            bool alive = true;
        };

        TransformHierarchy real;
        ReferenceHierarchy reference;
        std::vector<Pair> pairs;
        // living nodes only, so a recycled address never resolves to a dead pair
        std::unordered_map<const RefNode*, usize> indexOfNode;
        std::vector<usize> living;

    public:
        usize Create(usize parent, const Transform& local = Transform::Identity()){
            auto handle = parent == NO_PARENT ?
                real.CreateNode(local) :
                real.CreateNode(pairs[parent].handle, local);
            auto* node = reference.CreateNode(
                parent == NO_PARENT ? nullptr : pairs[parent].node,
                local
            );

            pairs.push_back(Pair{.handle = handle, .node = node});
            indexOfNode.emplace(node, pairs.size() - 1);
            living.push_back(pairs.size() - 1);

            return pairs.size() - 1;
        }

        void Destroy(usize index){
            auto& pair = pairs[index];

            real.DestroyNode(pair.handle);
            reference.DestroyNode(pair.node);
            // the allocator hands the same address out again, so a dangling entry
            // here would make a later node look like this one
            indexOfNode.erase(pair.node);
            std::erase(living, index);
            pair.alive = false;
            pair.node = nullptr;
        }

        void DestroySubtree(usize index){
            auto victims = SubtreeOf(index);

            std::vector<TransformHandle> destroyed;
            real.DestroySubtree(pairs[index].handle, destroyed);
            EXPECT_EQ(destroyed.size(), victims.size());

            // leaf first, which is all the reference will accept
            for(auto victim=victims.rbegin(); victim!=victims.rend(); ++victim){
                auto& pair = pairs[*victim];

                reference.DestroyNode(pair.node);
                indexOfNode.erase(pair.node);
                std::erase(living, *victim);
                pair.alive = false;
                pair.node = nullptr;
            }
        }

        // preorder, so reversing it gives a leaf first order
        std::vector<usize> SubtreeOf(usize index) const{
            std::vector<usize> subtree;
            collectSubtree(pairs[index].node, subtree);

            return subtree;
        }

        void SetParent(usize index, usize parent){
            real.SetParent(
                pairs[index].handle,
                parent == NO_PARENT ?
                    TransformHandle::InvalidHandle() :
                    pairs[parent].handle
            );
            reference.SetParent(
                pairs[index].node,
                parent == NO_PARENT ? nullptr : pairs[parent].node
            );
        }

        void SetLocal(usize index, const Transform& local){
            real.SetLocalTransform(pairs[index].handle, local);
            pairs[index].node->local = local;
        }

        void Commit(){
            real.CommitStructuralChanges();

            Str error;
            ASSERT_TRUE(CheckInvariants(real, &error)) << error;
        }

        void Update(){
            real.UpdateWorldTransforms();
            reference.UpdateWorldTransforms();
        }

        void ExpectSameShape() const{
            ASSERT_EQ(real.Size(), reference.Size());

            for(const auto& pair: pairs){
                if(!pair.alive){
                    EXPECT_FALSE(real.IsValid(pair.handle));
                    continue;
                }

                ASSERT_TRUE(real.IsValid(pair.handle));
                EXPECT_EQ(real.GetChildCount(pair.handle), pair.node->children.size());

                auto parent = real.GetParent(pair.handle);
                if(pair.node->parent){
                    ASSERT_TRUE(parent.IsValid());
                    EXPECT_EQ(parent, handleOf(pair.node->parent));
                }
                else{
                    EXPECT_FALSE(parent.IsValid());
                }
            }
        }

        void ExpectSameWorlds() const{
            for(const auto& pair: pairs){
                if(!pair.alive){
                    continue;
                }

                expectNearMatrix(
                    real.GetWorldMatrix(pair.handle),
                    pair.node->world,
                    pair.node
                );
                // a quaternion and its negation are the same rotation
                EXPECT_NEAR(
                    std::abs(dot(
                        real.GetWorldRotation(pair.handle),
                        pair.node->worldRotation
                    )),
                    1.0f, EPSILON
                ) << "node " << pair.node;
            }
        }

        void ExpectAgreement(){
            Commit();
            Update();
            ExpectSameShape();
            ExpectSameWorlds();
        }

        usize LivingCount() const noexcept{
            return living.size();
        }
        const std::vector<usize>& LivingIndexes() const noexcept{
            return living;
        }
        bool IsLeaf(usize index) const noexcept{
            return pairs[index].node->children.empty();
        }
        usize ParentOf(usize index) const noexcept{
            const auto* parent = pairs[index].node->parent;
            if(!parent){
                return NO_PARENT;
            }

            return indexOfNode.at(parent);
        }
        bool IsInSubtreeOf(usize root, usize candidate) const noexcept{
            for(auto* walk=pairs[candidate].node; walk; walk=walk->parent){
                if(walk == pairs[root].node){
                    return true;
                }
            }

            return false;
        }

    private:
        void collectSubtree(const RefNode* node, std::vector<usize>& out) const{
            out.push_back(indexOfNode.at(node));

            for(const auto* child: node->children){
                collectSubtree(child, out);
            }
        }

        TransformHandle handleOf(const RefNode* node) const noexcept{
            auto found = indexOfNode.find(node);

            return found == indexOfNode.end() ?
                TransformHandle::InvalidHandle() :
                pairs[found->second].handle;
        }

        static void expectNearMatrix(
            const Mat4& lhs, const Mat4& rhs, const RefNode* node
        ){
            for(usize column=0; column<4; ++column){
                for(usize row=0; row<4; ++row){
                    EXPECT_NEAR(lhs[column][row], rhs[column][row], EPSILON)
                        << "node " << node
                        << " at column " << column << ", row " << row;
                }
            }
        }
    };
}
