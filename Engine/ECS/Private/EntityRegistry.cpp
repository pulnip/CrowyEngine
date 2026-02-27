#include "EntityRegistry.hpp"
#include "Component.hpp"

namespace Crowy
{
    bool EntityRegistry::destroyEntity(EntityID id){
        auto entity_it = entityTable.find(id);

        CROWY_ASSERT(entity_it != entityTable.end(),
            "Cannot destroy entity: Entity {} does not exist", id);
        if(entity_it == entityTable.end())
            return false;

        const auto& info = entity_it->second;
        auto mapIt = archetypeMap.find(info.bit);

        CROWY_ASSERT(mapIt != archetypeMap.end(),
            "Cannot destroy entity: Missing ArchetypeVector (archetype: {:#x})",
            info.bit);
        if(mapIt == archetypeMap.end())
            return false;

        auto& table = mapIt->second;
        auto row = table[info.tableIndex];
        row = table.back();

        EntityID backEntityId = row.get<EntityID>(ID_REGION);
        entityTable.at(backEntityId).tableIndex = info.tableIndex;

        table.pop_back();

        return true;
    }

    std::optional<EntityHandle> EntityRegistry::query(EntityID id) noexcept{
        auto it = entityTable.find(id);
        if(it == entityTable.end()){
            return std::nullopt;
        }
        const auto& info = it->second;

        auto mapIt = archetypeMap.find(info.bit);
        CROWY_ASSERT(mapIt != archetypeMap.end(),
            "Cannot query entity: Missing ArchetypeVector (archetype: {:#x})",
            info.bit);
        if(mapIt == archetypeMap.end()){
            return std::nullopt;
        }
        auto& table = mapIt->second;

        return EntityHandle{
            .ptr = table[info.tableIndex],
            .bit = info.bit
        };
    }

    ArchetypeTable& EntityRegistry::getVector(ArchetypeBit bit){
        auto it = archetypeMap.find(bit);
        if(it != archetypeMap.end())
            return it->second;

        auto[newIt, _] = archetypeMap.try_emplace(bit, sizeof(EntityID) + size_of(bit));
        return newIt->second;
    }
}