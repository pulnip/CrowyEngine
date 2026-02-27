#pragma once

#include <cstdint>
#include <utility>
#include "concepts.hpp"
#include "ComponentDefinitions.hpp"

namespace Crowy
{
    using ArchetypeBit = uint64_t;
    using EntityID = uint32_t;

    inline constexpr bool isSubset(ArchetypeBit lhs, ArchetypeBit rhs){
        return (lhs & rhs) == lhs;
    }

    #define X(type) static_assert(std::is_trivially_copyable_v<type>);
    ARCHETYPES
    #undef X

    template<typename T>
    consteval bool isBuiltIn(){ return false; }
    #define X(type) template<> \
        consteval bool isBuiltIn<type>(){ return true; }
    ARCHETYPES
    #undef X

    enum{
        #define X(type) OrdinalOf##type,
        ARCHETYPES
        #undef X
        NUM_ARCHETYPES
    };

    #define X(type) constexpr ArchetypeBit \
        BitOf##type = (ArchetypeBit(1) << OrdinalOf##type);
    ARCHETYPES
    #undef X

    template<typename T>
    consteval ArchetypeBit bit_of(){
        return 0;
    }
    template<typename... Ts>
    consteval ArchetypeBit bits_of(){
        return (... | bit_of<Ts>());
    }
    #define X(type) template<> \
        consteval ArchetypeBit bit_of<type>(){ return BitOf##type; }
    ARCHETYPES
    #undef X

    template<value_type T>
    auto bits_of(T)->ArchetypeBit{
        using U = std::remove_cvref_t<T>;

        return bit_of<U>();
    }
    template<value_type T1, all_value... TN>
    auto bits_of(T1, TN... tn){
        using U = std::remove_cvref_t<T1>;

        return bit_of<U>() + bits_of(tn...);
    }

    template<pointer_type T>
    auto bits_of(T t){
        using U = std::remove_pointer_t<std::remove_cvref_t<T>>;

        return t != nullptr ? bit_of<U>() : 0;
    }
    template<pointer_type T1, all_pointer... TN>
    auto bits_of(T1 t1, TN... tn){
        using U = std::remove_pointer_t<std::remove_cvref_t<T1>>;
        auto bit = bits_of(tn...);

        return bit + (t1 != nullptr ? bit_of<U>() : 0);
    }

    template<optional_type T>
    auto bits_of(const T& t){
        using U = remove_optional_t<std::remove_cvref_t<T>>;

        return t.has_value() ? bit_of<U>() : 0;
    }
    template<optional_type T1, all_optional... TN>
    auto bits_of(const T1& t1, const TN&... tn){
        using U = remove_optional_t<std::remove_cvref_t<T1>>;
        auto bit = bits_of(tn...);

        return bit + (t1.has_value() ? bit_of<U>() : 0);
    }

    constexpr size_t size_of(ArchetypeBit bit){
        size_t size = 0;
        #define X(type) \
            if(bit & BitOf##type) \
                size += sizeof(type);
        ARCHETYPES
        #undef X
        return size;
    }

    template<typename T>
    constexpr size_t offset_of(ArchetypeBit bit){
        if(!isSubset(bit_of<T>(), bit))
            return -1;

        size_t offset = 0;
        #define X(type) \
            if(std::same_as<T, type>) \
                return offset; \
            if(bit & bit_of<type>()) \
                offset += sizeof(type);
        ARCHETYPES
        #undef X
        return offset;
    }

    constexpr std::string name_of(ArchetypeBit bit){
        switch(bit) {
        #define X(type) \
        case BitOf##type: \
            return #type;
        ARCHETYPES
        #undef X
        default:
            std::unreachable();
        }
    }

    template<typename T>
    constexpr std::string name_of(){
        return name_of(bit_of<T>());
    }
}
