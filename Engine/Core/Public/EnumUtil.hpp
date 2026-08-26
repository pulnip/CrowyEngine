#pragma once

#include <array>
#include <optional>
#include <type_traits>
#include "Primitives.hpp"

namespace Crowy
{
    template<typename T>
    concept EnumType = std::is_enum_v<T>;

    template<EnumType T>
    struct EnumEntry{
        CStr name;
        T value;
    };

    // declared once per enum through CROWY_ENUM_BEGIN below, which fills:
    //   static constexpr CStr name;
    //   static constexpr std::array entries;  // of EnumEntry<T>
    // every lookup here and every consumer derives from those two
    template<EnumType T>
    struct EnumTraits;

    template<typename T>
    concept HasEnumTraits = EnumType<T> && requires{
        EnumTraits<T>::name;
        EnumTraits<T>::entries.size();
    };

    template<EnumType T>
    inline constexpr CStr enumName(T value){
        for(const auto& entry: EnumTraits<T>::entries){
            if(entry.value == value){
                return entry.name;
            }
        }
        return nullptr;
    }

    template<EnumType T>
    inline constexpr std::optional<T> enumFromName(StrView name){
        for(const auto& entry: EnumTraits<T>::entries){
            if(name == entry.name){
                return entry.value;
            }
        }
        return std::nullopt;
    }

    template<EnumType E>
    constexpr bool hasFlag(E flags, E test){
        using U = std::underlying_type_t<E>;
        return (static_cast<U>(flags) & static_cast<U>(test)) != 0;
    }

    template<EnumType E>
    constexpr bool hasAll(E flags, E test){
        using U = std::underlying_type_t<E>;
        return (static_cast<U>(flags) & static_cast<U>(test)) == static_cast<U>(test);
    }

    template<EnumType E, EnumType... Es>
    constexpr E combine(E first, Es... rest){
        using U = std::underlying_type_t<E>;
        return static_cast<E>((
            static_cast<U>(first) | ... | static_cast<U>(rest)
        ));
    }
}

// Declares EnumTraits<TYPE> from the enumerators themselves, so an entry's
// string and its value cannot disagree. Write it inside namespace Crowy;
// the enum itself may live anywhere.
//
//   CROWY_ENUM_BEGIN(RHIBackend)
//       CROWY_ENUM_VALUE(DirectX12)
//       CROWY_ENUM_VALUE(Metal)
//   CROWY_ENUM_END()
#define CROWY_ENUM_BEGIN(TYPE) \
    template<> \
    struct EnumTraits<TYPE>{ \
        using Type = TYPE; \
        static constexpr CStr name = #TYPE; \
        static constexpr std::array entries{

#define CROWY_ENUM_VALUE(VALUE) \
            EnumEntry<Type>{#VALUE, Type::VALUE},

#define CROWY_ENUM_END() \
        }; \
    };
