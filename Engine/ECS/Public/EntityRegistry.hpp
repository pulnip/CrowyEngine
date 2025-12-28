#pragma once

#include <functional>
#include <optional>
#include <unordered_map>
#include "dynamic_vector.hpp"
#include "semantics.hpp"
#include "Component.hpp"
#include "ECSDefinitions.hpp"
#include "Log.hpp"

namespace Crowy
{
    template<typename... Ts>
    struct ArchetypeView{
        using Map = std::unordered_map<ArchetypeBit, dynamic_vector>;

    private:
        Map&         map;
        static constexpr ArchetypeBit required_bit = bits_of<Ts...>();

    public:
        struct sentinel{};
        struct iterator{
        private:
            Map::iterator map_it;
            Map::iterator map_end;
            Index         vec_index = 0;
            static constexpr ArchetypeBit required_bit = ArchetypeView::required_bit;

        public:
            iterator(Map::iterator map_it, Map::iterator map_end)
            :map_it(map_it), map_end(map_end){
                advance_to_valid_archetype();
            }

            auto operator*(){
                assert(!at_end());
                auto bit = map_it->first;
                auto& vec = map_it->second;
                assert(vec_index < vec.size());
                auto chunk_ptr = vec[vec_index];

                return std::forward_as_tuple(
                    *static_cast<EntityID*>(chunk_ptr),
                    map_it->first,
                    *static_cast<Ts*>(
                        ptrAdd(chunk_ptr, offset_of<Ts>(bit))
                    )...
                );
            }
            auto operator++()->iterator&{
                auto& vec = map_it->second;
                ++vec_index;
                if(vec_index >= vec.size()){
                    vec_index = 0;
                    ++map_it;
                    advance_to_valid_archetype();
                }
                return *this;
            }
            auto operator==(sentinel) noexcept{
                return map_it == map_end;
            }
            auto operator!=(sentinel) noexcept{
                return !((*this)==sentinel{});
            }

        private:
            void advance_to_valid_archetype(){
                while(map_it != map_end){
                    if( isSubset(required_bit, map_it->first) &&
                        map_it->second.size() > 0)
                        return;
                    ++map_it;
                }
            }
            auto at_end() const noexcept{ return map_it == map_end; }
        };
        struct const_iterator{
        private:
            Map::const_iterator map_it;
            Map::const_iterator map_end;
            Index               vec_index = 0;
            static constexpr ArchetypeBit required_bit = ArchetypeView::required_bit;

        public:
            const_iterator(Map::const_iterator map_it, Map::const_iterator map_end)
            :map_it(map_it), map_end(map_end){
                advance_to_valid_archetype();
            }

            auto operator*(){
                assert(!at_end());
                auto bit = map_it->first;
                auto& vec = map_it->second;
                assert(vec_index < vec.size());
                auto chunk_ptr = vec[vec_index];

                return std::forward_as_tuple(
                    *static_cast<const EntityID*>(chunk_ptr),
                    map_it->first,
                    *static_cast<Ts*>(
                        ptrAdd(chunk_ptr, offset_of<Ts>(bit))
                    )...
                );
            }
            auto operator++()->const_iterator&{
                auto& vec = map_it->second;
                ++vec_index;
                if(vec_index >= vec.size()){
                    vec_index = 0;
                    ++map_it;
                    advance_to_valid_archetype();
                }
                return *this;
            }
            auto operator==(sentinel) noexcept{
                return map_it == map_end;
            }
            auto operator!=(sentinel) noexcept{
                return !((*this)==sentinel{});
            }

        private:
            void advance_to_valid_archetype(){
                while(map_it != map_end){
                    if(isSubset(required_bit, map_it->first) &&
                        map_it->second.size() > 0)
                        return;
                    ++map_it;
                }
            }
            auto at_end() const noexcept{ return map_it == map_end; }
        };

        ArchetypeView(Map& map):map(map){}

        auto  begin() noexcept{ return iterator{map.begin(), map.end()}; }
        auto    end() noexcept{ return sentinel{}; }
        auto  begin() const noexcept{ return const_iterator{map.begin(), map.end()}; }
        auto    end() const noexcept{ return sentinel{}; }
        auto cbegin() const noexcept{ return const_iterator{map.begin(), map.end()}; }
        auto   cend() const noexcept{ return sentinel{}; }

        size_t size() const noexcept{
            size_t size = 0;
            for(auto it = map.cbegin(); it != map.cend(); ++it){
                if(isSubset(required_bit, it->first))
                    size += it->second.size();
            }

            return size;
        }
    };

    template<value_type T>
    void emplace_component(void* chunk, ArchetypeBit bit, T&& t){
        using U = std::remove_cvref_t<T>;

        auto offset = offset_of<U>(bit);
        auto dst = ptrAdd(chunk, offset);
        *static_cast<U*>(dst) = std::forward<T>(t);
    }
    template<value_type T1, all_value... TN>
    void emplace_component(void* chunk, ArchetypeBit bit,
        T1&& t1, TN&&... tn
    ){
        using U = std::remove_cvref_t<T1>;

        auto offset = offset_of<U>(bit);
        auto dst = ptrAdd(chunk, offset);
        *static_cast<U*>(dst) = std::forward<T1>(t1);

        emplace_component(chunk, bit, std::forward<TN>(tn)...);
    }
    template<pointer_type T>
    void emplace_component(void* chunk, ArchetypeBit bit, const T t){
        using U = std::remove_pointer_t<std::remove_cvref_t<T>>;

        auto offset = offset_of<U>(bit);
        auto dst = ptrAdd(chunk, offset);
        *static_cast<U>(dst) = *t;
    }
    template<pointer_type T1, all_pointer... TN>
    void emplace_component(void* chunk, ArchetypeBit bit,
        const T1 t1, const TN... tn
    ){
        using U = std::remove_pointer_t<std::remove_cvref_t<T1>>;

        auto offset = offset_of<U>(bit);
        auto dst = ptrAdd(chunk, offset);
        *static_cast<U>(dst) = *t1;

        emplace_component(chunk, bit, tn...);
    }
    template<optional_type T>
    void emplace_component(void* chunk, ArchetypeBit bit, T&& t){
        using U = remove_optional_t<std::remove_cvref_t<T>>;

        if(t.has_value()){
            auto offset = offset_of<U>(bit);
            auto dst = ptrAdd(chunk, offset);
            *static_cast<U*>(dst) = t.value();
        }
    }
    template<optional_type T1, all_optional... TN>
    void emplace_component(void* chunk, ArchetypeBit bit,
        const T1 t1, const TN... tn
    ){
        using U = remove_optional_t<std::remove_cvref_t<T1>>;

        if(t1.has_value()){
            auto offset = offset_of<U>(bit);
            auto dst = ptrAdd(chunk, offset);
            *static_cast<U*>(dst) = t1.value();
        }

        emplace_component(chunk, bit, tn...);
    }

    struct EntityInfo{
        ArchetypeBit bit;
        Index chunkIndex;
    };

    struct Entity{
        ArchetypeBit bit = 0;
        void* chunk = nullptr;
    };

    class EntityRegistry{
    private:
        using ArchetypeMap = std::unordered_map<ArchetypeBit, dynamic_vector>;
        using EntityTable = std::unordered_map<EntityID, EntityInfo>;

        ArchetypeMap archetypeMap;
        EntityTable entityTable;

        EntityID id_seed = 1;

    public:
        EntityRegistry() = default;
        ~EntityRegistry() = default;
        DECLARE_PINNED(EntityRegistry)

    private:
        auto issueID(){ return id_seed++; }

    public:
        template<typename... Args>
        auto createEntity(Args&&... args){
            auto bit = bits_of(args...);
            // auto bit = bits_of<remove_optional_t<std::remove_cvref_t<Args>>...>();

            if(archetypeMap.find(bit) == archetypeMap.end())
                archetypeMap.emplace(bit, size_of(bit));

            auto& vector = archetypeMap.at(bit);
            vector.resize(vector.size() + 1);
            auto index = vector.size() - 1;
            auto chunk = vector[index];

            auto entity_id = issueID();
            entityTable.emplace(entity_id, EntityInfo{
                .bit = bit, .chunkIndex = index
            });
            *static_cast<EntityID*>(chunk) = entity_id;
            emplace_component(chunk, bit, std::forward<Args>(args)...);

            return entity_id;
        }
        void destroyEntity(EntityID);

        template<typename... Ts>
        auto query(){
            return ArchetypeView<Ts...>(archetypeMap);
        }
        template<typename... Ts>
        auto query(EntityID id)->std::tuple<Ts&...>{
            const auto& info = entityTable.at(id);
            auto& vec = archetypeMap.at(info.bit);
            auto chunk = vec[info.chunkIndex];

            return std::forward_as_tuple(
                *static_cast<Ts*>(
                    ptrAdd(chunk, offset_of<Ts>(info.bit))
                )...
            );
        }
        template<typename T>
        auto query_safe(EntityID id)->std::optional<std::reference_wrapper<T>>{
            const auto& info = entityTable.at(id);
            if(!isSubset(bit_of<T>(), info.bit))
                return std::nullopt;

            auto& vec = archetypeMap.at(info.bit);
            auto chunk = vec[info.chunkIndex];
            return *static_cast<T*>(ptrAdd(chunk, offset_of<T>(info.bit)));
        }
        auto query(EntityID id)->Entity;

        template<typename T>
        void appendComponent(EntityID id, T&& component){
            auto entity_it = entityTable.find(id);
            if(entity_it == entityTable.end()){
                LOG_WARN(LOG_CORE, "Entity {} not exist. component cannot be added", id);
                return;
            }

            auto& info = entity_it->second;

            if(isSubset(bit_of<T>(), info.bit)){
                LOG_WARN(LOG_CORE, "Component {} already exist. (entity: {}, archetype: {})",
                    bit_of<T>(), id, info.bit);
                return;
            }

            auto [new_index, old_vec] = moveChunk(info,
                std::forward<T>(component));
            updateEntityInfo(info, old_vec,
                info.bit | bit_of<T>(), new_index
            );
        }
        template<typename T>
        void removeComponent(EntityID id){
            auto entity_it = entityTable.find(id);
            if(entity_it == entityTable.end()){
                LOG_WARN(LOG_CORE, "Entity {} not exist. component cannot be added", id);
                return;
            }

            auto& info = entity_it->second;

            if(!isSubset(bit_of<T>(), info.bit)){
                LOG_WARN(LOG_CORE, "{} not exist. (entity: {}, archetype: {})",
                    name_of<T>(), id, info.bit);
                return;
            }

            auto [new_index, old_vec] = moveChunk<T>(info);
            updateEntityInfo(info, old_vec,
                info.bit & (~bit_of<T>()), new_index);
        }

    private:
        auto getVector(ArchetypeBit)->dynamic_vector&;

        template<typename T>
        auto moveChunk(EntityInfo& info, T&& component){
            auto& old_vec = archetypeMap.at(info.bit);
            auto old_index = info.chunkIndex;
            auto chunk = old_vec[old_index];

            auto new_bit = info.bit | bit_of<T>();
            auto& new_vec = getVector(new_bit);

            new_vec.resize(new_vec.size() + 1);
            auto new_index = new_vec.size() - 1;
            auto dst = new_vec[new_index];

            // 1. copy new chunk
            // copy chunk before component
            dst = ptrWrite(dst, chunk, offset_of<T>(new_bit));
            chunk = ptrAdd(     chunk, offset_of<T>(new_bit));
            // copy component
            dst = ptrWrite(dst, std::forward<T>(component));
            // copy chunk after component
            ptrWrite(dst, chunk, size_of(info.bit) - offset_of<T>(new_bit));

            // 2. remove old chunk
            old_vec.swap_remove(info.chunkIndex);

            return std::tuple<size_t, dynamic_vector&>{new_index, old_vec};
        }
        template<typename T>
        auto moveChunk(EntityInfo& info){
            auto& old_vec = archetypeMap.at(info.bit);
            auto old_index = info.chunkIndex;
            auto chunk = old_vec[old_index];

            auto new_bit = info.bit & (~bit_of<T>());
            auto& new_vec = getVector(new_bit);

            new_vec.resize(new_vec.size() + 1);
            auto new_index = new_vec.size() - 1;
            auto dst = new_vec[new_index];

            // 1. copy new chunk
            // copy chunk before component
            dst = ptrWrite(dst, chunk, offset_of<T>(info.bit));
            // skip target component
            chunk = ptrAdd(chunk, offset_of<T>(info.bit) + sizeof(T));
            // copy chunk after component
            ptrWrite(dst, chunk, size_of(info.bit) - offset_of<T>(info.bit) - sizeof(T));

            // 2. remove old chunk
            old_vec.swap_remove(info.chunkIndex);

            return std::tuple<Index, dynamic_vector&>{new_index, old_vec};
        }

        void updateEntityInfo(EntityInfo& updated, dynamic_vector& swapped,
            ArchetypeBit updated_bit, Index updated_index);

        auto findEntityFromProperty(ArchetypeBit bit, Index chunkIndex)->EntityTable::iterator;
    };
}