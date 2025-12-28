#include <algorithm>
#include "EntityRegistry.hpp"

namespace Crowy
{
    void EntityRegistry::destroyEntity(EntityID id){
        auto entity_it = entityTable.find(id);
        if(entity_it == entityTable.end()){
            LOG_WARN(LOG_CORE, "Entity {} not exist.", id);
            return;
        }

        const auto& info = entity_it->second;
        auto arch_it = archetypeMap.find(info.bit);
        if(arch_it == archetypeMap.end()){
            LOG_FATAL(LOG_CORE, "Archetype of Entity {}: {}, but ArchetypeVector not exist", id, info.bit);
            return;
        }

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
    }

    auto EntityRegistry::query(EntityID id)->Entity{
        auto entity_it = entityTable.find(id);
        if(entity_it == entityTable.end()){
            LOG_WARN(LOG_CORE, "Entity {} not exist.", id);
            return {};
        }
        const auto& info = entity_it->second;

        auto arch_it = archetypeMap.find(info.bit);
        if(arch_it == archetypeMap.end()){
            LOG_FATAL(LOG_CORE, "Archetype of Entity {}: {}, but ArchetypeVector not exist", id, info.bit);
            return {};
        }
        auto& vec = arch_it->second;

        return Entity{
            .bit=info.bit,
            .chunk=vec[info.chunkIndex]
        };
    }

    auto EntityRegistry::getVector(ArchetypeBit bit)->dynamic_vector&{
        auto it = archetypeMap.find(bit);
        if(it != archetypeMap.end())
            return it->second;

        auto[new_it, _] = archetypeMap.try_emplace(bit, size_of(bit));
        return new_it->second;
    }

    void EntityRegistry::updateEntityInfo(EntityInfo& updated, dynamic_vector& swapped,
        ArchetypeBit updated_bit, Index updated_index
    ){
        if(swapped.size() > 0){
            auto it = findEntityFromProperty(updated.bit, swapped.size());

            if(it == entityTable.end()){
                throw std::runtime_error("Entity Table integrity Broken!");
            }

            auto& swapped_entity = it->second;
            swapped_entity.chunkIndex = updated.chunkIndex;
        }
        updated.bit = updated_bit;
        updated.chunkIndex = updated_index;
    }

    auto EntityRegistry::findEntityFromProperty(
        ArchetypeBit bit, Index chunkIndex
    )->EntityRegistry::EntityTable::iterator{
        // TODO. might be replace to entity tag component
        auto it = std::ranges::find_if(entityTable,
            [bit, chunkIndex](const auto& pair){
                const EntityInfo& info = pair.second;
                return info.bit == bit && info.chunkIndex == chunkIndex;
            }
        );
        if(it == entityTable.end()){
            LOG_FATAL(LOG_CORE, "Entity with archetype {}, index {} not in entity table!", bit, chunkIndex);
        }
        return it;
    }
}