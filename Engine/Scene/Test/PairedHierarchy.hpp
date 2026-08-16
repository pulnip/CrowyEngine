#pragma once

// Test only. Feeds one operation sequence to both implementations and holds them
// against each other.

#include <limits>
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

            return pairs.size() - 1;
        }

        void Destroy(usize index){
            auto& pair = pairs[index];

            real.DestroyNode(pair.handle);
            reference.DestroyNode(pair.node);
            pair.alive = false;
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
            }
        }

        void ExpectAgreement(){
            Commit();
            Update();
            ExpectSameShape();
            ExpectSameWorlds();
        }

        usize LivingCount() const noexcept{
            usize count = 0;
            for(const auto& pair: pairs){
                count += pair.alive ? 1 : 0;
            }

            return count;
        }
        std::vector<usize> LivingIndexes() const{
            std::vector<usize> living;
            for(usize i=0; i<pairs.size(); ++i){
                if(pairs[i].alive){
                    living.push_back(i);
                }
            }

            return living;
        }
        bool IsLeaf(usize index) const noexcept{
            return pairs[index].node->children.empty();
        }

    private:
        TransformHandle handleOf(const RefNode* node) const noexcept{
            for(const auto& pair: pairs){
                if(pair.node == node){
                    return pair.handle;
                }
            }

            return TransformHandle::InvalidHandle();
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
