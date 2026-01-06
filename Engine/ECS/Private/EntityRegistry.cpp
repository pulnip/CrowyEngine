#include <algorithm>
#include "EntityRegistry.hpp"

namespace Crowy
{
    bool EntityRegistry::destroyEntity(EntityID id){
        auto entity_it = entityTable.find(id);

        CROWY_ASSERT(entity_it != entityTable.end(),
            "Cannot destroy entity: Entity {} does not exist", id);
        if(entity_it == entityTable.end())
            return false;

        const auto& info = entity_it->second;
        auto arch_it = archetypeMap.find(info.bit);

        CROWY_ASSERT(arch_it != archetypeMap.end(),
            "Cannot destroy entity: Missing ArchetypeVector (archetype: {:#x})",
            info.bit);
        if(arch_it == archetypeMap.end())
            return false;

        auto& vec = arch_it->second;
        auto removedIndex = info.chunkIndex;

        if(vec.size() > 1 && removedIndex != vec.size() - 1){
            auto swapped_it = findEntityFromProperty(info.bit, vec.size() - 1);
            if(swapped_it != entityTable.end()){
                swapped_it->second.chunkIndex = removedIndex;
            }
        }

        vec.swap_remove(info.chunkIndex);
        entityTable.erase(id);

        return true;
    }

    std::optional<Entity> EntityRegistry::query(EntityID id) noexcept{
        auto entity_it = entityTable.find(id);
        if(entity_it == entityTable.end()){
            return std::nullopt;
        }
        const auto& info = entity_it->second;

        auto arch_it = archetypeMap.find(info.bit);
        CROWY_ASSERT(arch_it != archetypeMap.end(),
            "Cannot query entity: Missing ArchetypeVector (archetype: {:#x})",
            info.bit);
        if(arch_it == archetypeMap.end()){
            return std::nullopt;
        }
        auto& vec = arch_it->second;

        return Entity{
            .bit=info.bit,
            .chunk=vec[info.chunkIndex]
        };
    }

    dynamic_vector& EntityRegistry::getVector(ArchetypeBit bit){
        auto it = archetypeMap.find(bit);
        if(it != archetypeMap.end())
            return it->second;

        auto[new_it, _] = archetypeMap.try_emplace(bit, size_of(bit));
        return new_it->second;
    }

    void EntityRegistry::updateEntityInfo(
        EntityInfo& updated, dynamic_vector& swapped,
        ArchetypeBit updated_bit, Index updated_index
    ) noexcept{
        if(swapped.size() > 0){
            auto it = findEntityFromProperty(updated.bit, swapped.size());

            auto& swapped_entity = it->second;
            swapped_entity.chunkIndex = updated.chunkIndex;
        }
        updated.bit = updated_bit;
        updated.chunkIndex = updated_index;
    }

    EntityRegistry::EntityTable::iterator EntityRegistry::findEntityFromProperty(
        ArchetypeBit bit, Index chunkIndex
    ) noexcept{
        // TODO. might be replace to entity tag component
        auto it = std::ranges::find_if(entityTable,
            [bit, chunkIndex](const auto& pair){
                const EntityInfo& info = pair.second;
                return info.bit == bit && info.chunkIndex == chunkIndex;
            }
        );

        CROWY_ASSERT(it != entityTable.end(),
            "Entity Table integrity Broken!");

        return it;
    }
}