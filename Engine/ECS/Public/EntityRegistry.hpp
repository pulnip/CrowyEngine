#pragma once

#include <optional>
#include <tuple>
#include <unordered_map>
#include "assert.hpp"
#include "typeless_vector.hpp"
#include "semantics.hpp"
#include "Component.hpp"
#include "EntityHandle.hpp"

namespace Crowy
{
    using ArchetypeTable = typeless_vector<0>;
    using ArchetypeMap = std::unordered_map<ArchetypeBit, ArchetypeTable>;

    template<typename... Ts>
    struct ArchetypeView{
    private:
        static constexpr auto requiredBit = bits_of<Ts...>();
        ArchetypeMap& map;

    public:
        struct sentinel{};
        struct iterator{
        private:
            using MapIterator = ArchetypeMap::iterator;

            MapIterator mapIt;
            size_t tableIndex;
            const MapIterator mapEnd;
            static constexpr auto requiredBit = ArchetypeView::requiredBit;

        public:
            iterator(MapIterator mapIt, size_t tableIndex, MapIterator mapEnd)
                : mapIt(mapIt), tableIndex(tableIndex), mapEnd(mapEnd)
            {
                toValidArchetype();
            }

            std::tuple<EntityID, ArchetypeBit, Ts&...> operator*() noexcept{
                CROWY_ASSERT(mapIt != mapEnd);
                auto bit = mapIt->first;
                auto& table = mapIt->second;
                CROWY_ASSERT(tableIndex < table.size());

                type_proxy<0> row = table[tableIndex];
                EntityID id = row.get<EntityID>(0);

                return {id, bit,
                    row.get<Ts>(sizeof(EntityID) + offset_of<Ts>(bit))...
                };
            }
            iterator& operator++() noexcept{
                auto& table = mapIt->second;
                ++tableIndex;
                if(tableIndex >= table.size()){
                    ++mapIt;
                    tableIndex = 0;
                    toValidArchetype();
                }
                return *this;
            }
            auto operator==(sentinel) noexcept{
                return atEnd();
            }
            auto operator!=(sentinel) noexcept{
                return !((*this)==sentinel{});
            }

        private:
            void toValidArchetype() noexcept{
                while(!atEnd()){
                    const auto bit = mapIt->first;
                    const auto& row = mapIt->second;

                    if(isSubset(requiredBit, bit) && row.size() > 0)
                        return;
                    ++mapIt;
                }
            }
            auto atEnd() const noexcept{ return mapIt == mapEnd; }
        };
        struct const_iterator{
        private:
            using MapIterator = ArchetypeMap::const_iterator;

            MapIterator mapIt;
            size_t tableIndex;
            const MapIterator mapEnd;
            static constexpr auto requiredBit = ArchetypeView::requiredBit;

        public:
            const_iterator(MapIterator mapIt, size_t tableIndex, MapIterator mapEnd)
                : mapIt(mapIt), tableIndex(tableIndex), mapEnd(mapEnd)
            {
                toValidArchetype();
            }

            std::tuple<EntityID, ArchetypeBit, const Ts&...> operator*() noexcept{
                CROWY_ASSERT(mapIt != mapEnd);
                auto bit = mapIt->first;
                auto& table = mapIt->second;
                CROWY_ASSERT(tableIndex < table.size());

                const_proxy<0> row = table[tableIndex];
                EntityID id = row.get<EntityID>(0);

                return {id, bit,
                    row.get<Ts>(sizeof(EntityID) + offset_of<Ts>(bit))...
                };
            }
            const_iterator& operator++() noexcept{
                auto& table = mapIt->second;
                ++tableIndex;
                if(tableIndex >= table.size()){
                    ++mapIt;
                    tableIndex = 0;
                    toValidArchetype();
                }
                return *this;
            }
            auto operator==(sentinel) noexcept{
                return atEnd();
            }
            auto operator!=(sentinel) noexcept{
                return !((*this)==sentinel{});
            }

        private:
            void toValidArchetype() noexcept{
                while(!atEnd()){
                    const auto bit = mapIt->first;
                    const auto& row = mapIt->second;

                    if(isSubset(requiredBit, bit) && row.size() > 0)
                        return;
                    ++mapIt;
                }
            }
            auto atEnd() const noexcept{ return mapIt == mapEnd; }
        };

        ArchetypeView(ArchetypeMap& map):map(map){}

        auto begin() noexcept{
            return iterator{map.begin(), 0, map.end()};
        }
        auto end() noexcept{
            return sentinel{};
        }
        auto begin() const noexcept{
            return const_iterator{map.begin(), 0, map.end()};
        }
        auto end() const noexcept{
            return sentinel{};
        }
        auto cbegin() const noexcept{
            return const_iterator{map.begin(), 0, map.end()};
        }
        auto cend() const noexcept{
            return sentinel{};
        }

        size_t size() const noexcept{
            size_t size = 0;
            for(const auto& [bit, table]: map){
                if(isSubset(requiredBit, bit))
                    size += table.size();
            }

            return size;
        }
    };

    template<typename T>
        requires std::is_trivially_copyable_v<
            remove_optional_t<std::remove_pointer_t<std::remove_cvref_t<T>>>>
    void emplaceComponent(void* dst, ArchetypeBit bit, T&& t) noexcept{
        using U = remove_optional_t<std::remove_pointer_t<std::remove_cvref_t<T>>>;
        dst = ptr_add(dst, offset_of<U>(bit));

        if constexpr(value_type<T>)
            std::memcpy(dst, &t, sizeof(U));
        else if constexpr(pointer_type<T>){
            if(t != nullptr)
                std::memcpy(dst, t, sizeof(U));
        }
        else if constexpr(optional_type<T>){
            if(t.has_value())
                std::memcpy(dst, &t.value(), sizeof(U));
        }
    }

    template<typename T1, typename... TN>
        requires std::is_trivially_copyable_v<
            remove_optional_t<std::remove_pointer_t<std::remove_cvref_t<T1>>>>
    void emplaceComponent(void* dst, ArchetypeBit bit, T1&& t1, TN&&... tn){
        using U = remove_optional_t<std::remove_pointer_t<std::remove_cvref_t<T1>>>;

        emplaceComponent(dst, bit, std::forward<T1>(t1));
        emplaceComponent(dst, bit, std::forward<TN>(tn)...);
    }

    struct EntityIndex{
        ArchetypeBit bit;
        size_t tableIndex;
    };

    class EntityRegistry{
    private:
        ArchetypeMap archetypeMap;
        static constexpr size_t ID_REGION = 0;
        static constexpr auto COMPONENT_REGION = ID_REGION + sizeof(EntityID);
        std::unordered_map<EntityID, EntityIndex> entityTable;

        EntityID id_seed = 1;

    public:
        EntityRegistry() = default;
        ~EntityRegistry() = default;
        CROWY_DECLARE_PINNED(EntityRegistry)

    private:
        auto issueID() noexcept{ return id_seed++; }

    public:
        template<typename... Args>
        EntityID createEntity(Args&&... args){
            auto bit = bits_of(args...);
            // auto bit = bits_of<remove_optional_t<std::remove_cvref_t<Args>>...>();
            auto& table = getVector(bit);
            auto entityId = issueID();
            auto tableIndex = table.size();

            entityTable.emplace(entityId, EntityIndex{
                .bit = bit, .tableIndex = tableIndex
            });
            table.resize(table.size() + 1);

            // packing [EntityID + Components] layout
            auto dst = table[tableIndex].data;
            mem_pack(ptr_add(dst, ID_REGION), entityId);
            emplaceComponent(ptr_add(dst, COMPONENT_REGION), bit,
                std::forward<Args>(args)...
            );

            return entityId;
        }
        bool destroyEntity(EntityID);

        template<typename... Ts>
        auto query() noexcept{
            return ArchetypeView<Ts...>(archetypeMap);
        }
        template<typename... Ts>
        std::tuple<Ts&...> query_unsafe(EntityID id) noexcept{
            const auto& info = entityTable.at(id);
            auto& table = archetypeMap.at(info.bit);
            auto row = table[info.tableIndex];

            return std::make_tuple(
                row.get<Ts>(sizeof(EntityID) + offset_of<Ts>(info.bit))...
            );
        }
        template<typename T>
        std::optional<type_proxy<sizeof(T)>> query(EntityID id) noexcept{
            const auto& info = entityTable.at(id);
            if(!isSubset(bit_of<T>(), info.bit))
                return std::nullopt;

            auto& table = archetypeMap.at(info.bit);
            auto row = table[info.tableIndex];
            return row.get<T>(sizeof(EntityID) + offset_of<T>(info.bit));
        }
        std::optional<EntityHandle> query(EntityID id) noexcept;

        template<typename T>
        bool appendComponent(EntityID id, T&& component){
            auto entityIt = entityTable.find(id);

            CROWY_ASSERT(entityIt != entityTable.end(),
                "Cannot add component: Entity {} does not exist", id);
            if(entityIt == entityTable.end())
                return false;

            auto& info = entityIt->second;

            CROWY_ASSERT(!isSubset(bit_of<T>(), info.bit),
                "Cannot add component: {} already exists on entity {} (archetype: {:#x})",
                name_of<T>(), id, info.bit);
            if(isSubset(bit_of<T>(), info.bit))
                return false;

            auto srcBit = info.bit;
            auto& srcTable = archetypeMap.at(info.bit);
            auto srcTableIndex = info.tableIndex;
            auto srcRow = srcTable[srcTableIndex];

            ArchetypeBit dstBit = srcBit | bit_of<T>();
            ArchetypeTable& dstTable = getVector(dstBit);
            auto dstTableIndex = dstTable.size();
            dstTable.resize(dstTable.size() + 1);
            auto dstRow = dstTable[dstTableIndex];

            size_t midFirst = ID_REGION + sizeof(EntityID) + offset_of<T>(dstBit);
            size_t remain = size_of(dstBit) - (offset_of<T>(dstBit) + sizeof(T));
            dstRow.get(ID_REGION, midFirst) = srcRow.get(ID_REGION, midFirst);
            dstRow.get<T>(midFirst) = std::forward<T>(component);
            dstRow.get(midFirst + sizeof(T), remain) = srcRow.get(midFirst, remain);

            info.tableIndex = dstTableIndex;

            // fill empty row to last element
            srcRow = srcTable.back();
            EntityID backEntityId = srcRow.get<EntityID>(ID_REGION);
            entityTable.at(backEntityId).tableIndex = srcTableIndex;

            srcTable.pop_back();

            return true;
        }
        template<typename T>
        bool removeComponent(EntityID id){
            auto entity_it = entityTable.find(id);

            CROWY_ASSERT(entity_it != entityTable.end(),
                "Cannot remove component: Entity {} does not exist", id);
            if(entity_it == entityTable.end())
                return false;

            auto& info = entity_it->second;

            CROWY_ASSERT(isSubset(bit_of<T>(), info.bit),
                "Cannot remove component: {} does not exist on entity {} (archetype: {:#x})",
                name_of<T>(), id, info.bit);
            if(!isSubset(bit_of<T>(), info.bit))
                return false;

            auto srcBit = info.bit;
            auto& srcTable = archetypeMap.at(info.bit);
            auto srcTableIndex = info.tableIndex;
            auto srcRow = srcTable[srcTableIndex];

            ArchetypeBit dstBit = srcBit & ~bit_of<T>();
            ArchetypeTable& dstTable = getVector(dstBit);
            auto dstTableIndex = dstTable.size();
            dstTable.resize(dstTable.size() + 1);
            auto dstRow = dstTable[dstTableIndex];

            size_t midFirst = ID_REGION + sizeof(EntityID) + offset_of<T>(srcBit);
            size_t remain = size_of(srcBit) - (offset_of<T>(srcBit) + sizeof(T));
            dstRow.get(ID_REGION, midFirst) = srcRow.get(ID_REGION, midFirst);
            dstRow.get(midFirst, remain) = srcRow.get(midFirst + sizeof(T), remain);

            info.tableIndex = dstTableIndex;

            // fill empty row to last element
            srcRow = srcTable.back();
            EntityID backEntityId = srcRow.get<EntityID>(ID_REGION);
            entityTable.at(backEntityId).tableIndex = srcTableIndex;

            srcTable.pop_back();

            return true;
        }

    private:
        ArchetypeTable& getVector(ArchetypeBit);
    };
}