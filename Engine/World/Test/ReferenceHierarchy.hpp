#pragma once

// Test only. Speed does not matter here, being obviously right does.

#include <memory>
#include <vector>
#include "Assert.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    struct RefNode{
        Transform local = Transform::Identity();
        Mat4 world = unitMat();
        Vec4 worldRotation = unitQuat();
        RefNode* parent = nullptr;
        std::vector<RefNode*> children;
    };

    class ReferenceHierarchy{
    private:
        std::vector<std::unique_ptr<RefNode>> owned;

    public:
        RefNode* CreateNode(RefNode* parent, const Transform& local){
            auto held = std::make_unique<RefNode>(RefNode{
                .local = local,
                .parent = parent
            });
            auto* node = held.get();
            owned.push_back(std::move(held));

            if(parent){
                parent->children.push_back(node);
            }

            return node;
        }

        void SetParent(RefNode* node, RefNode* parent){
            for(auto* walk=parent; walk; walk=walk->parent){
                CROWY_ASSERT(walk != node);
            }

            if(node->parent){
                std::erase(node->parent->children, node);
            }
            node->parent = parent;
            if(parent){
                parent->children.push_back(node);
            }
        }

        void DestroyNode(RefNode* node){
            CROWY_ASSERT(node->children.empty());

            if(node->parent){
                std::erase(node->parent->children, node);
            }
            std::erase_if(owned, [node](const auto& held){
                return held.get() == node;
            });
        }

        void UpdateWorldTransforms(){
            for(auto& held: owned){
                if(!held->parent){
                    update(held.get(), unitMat(), unitQuat());
                }
            }
        }

        usize Size() const noexcept{
            return owned.size();
        }

    private:
        void update(RefNode* node, const Mat4& parentWorld, Vec4 parentRotation){
            node->world = parentWorld * modelMat(node->local);
            node->worldRotation = quat(parentRotation, node->local.rotation);

            for(auto* child: node->children){
                update(child, node->world, node->worldRotation);
            }
        }
    };
}
