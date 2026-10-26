#pragma once

#include <array>
#include <memory>
#include <ranges>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "Assert.hpp"
#include "Primitives.hpp"
#include "PropertyDesc.hpp"
#include "StringUtil.hpp"
#include "TypeOps.hpp"

namespace Crowy
{
    namespace detail
    {
        template<typename T>
        struct IsVector: std::false_type {};
        template<typename E, typename A>
        struct IsVector<std::vector<E, A>>: std::true_type {};
    }

    // a property type reflected as a list of its elements
    template<typename T>
    concept ListType = detail::IsVector<T>::value;

    template<typename T>
    const TypeOps* getTypeOps();

    struct TypeDesc {
        Str name;
        const TypeDesc* parent = nullptr;
        // what creating the type by name goes through
        const TypeOps* ops = nullptr;
        // registration order, which consumers treat as declaration order
        std::vector<PropertyDesc> properties;
        StringHashMap<usize> indexByName;

        template<typename... Ptrs>
            requires detail::MemberPointers<Ptrs...>
        PropertyDesc& AddProperty(CStr name, Ptrs... ptrs) {
            using Leaf = typename detail::MemberChain<Ptrs...>::Leaf;

            static_assert(
                HasTypeTraits<Leaf> || ListType<Leaf> ||
                    (std::is_class_v<Leaf> && !std::ranges::range<Leaf>),
                "of containers only std::vector is reflected; register a "
                "leaf type, a reflectable struct or a std::vector of one"
            );

            auto [it, ret] = indexByName.try_emplace(name, properties.size());
            CROWY_ASSERT(ret);

            properties.push_back(
                PropertyDesc{
                    .name = name,
                    .type = *getTypeOps<Leaf>(),
                    .accessor =
                        std::make_unique<MemberAccessor<Ptrs...>>(ptrs...),
                    .meta = {}
                }
            );

            return properties.back();
        }

        const PropertyDesc* Find(StrView name) const {
            auto it = indexByName.find(name);
            return it == indexByName.end() ? nullptr : &properties[it->second];
        }

        // own properties shadow the parent chain's
        const PropertyDesc* FindInChain(StrView name) const {
            for(const auto* desc = this; desc != nullptr; desc = desc->parent) {
                if(const auto* prop = desc->Find(name)) {
                    return prop;
                }
            }
            return nullptr;
        }
    };

    class ClassRegistry {
    private:
        std::unordered_map<std::type_index, RAII<TypeDesc>> descByTypeindex;
        StringHashMap<TypeDesc*> typeByName;

    public:
        static ClassRegistry& Get();

        // nullptr for a name no registration gave
        static const TypeDesc* FindType(StrView name);

        // creates the slot on demand, and its address stays valid
        template<typename T>
        TypeDesc& DescFor() {
            auto& slot = descByTypeindex[std::type_index(typeid(T))];

            if(slot == nullptr) {
                slot = std::make_unique<TypeDesc>();
                slot->ops = getTypeOps<T>();
            }

            return *slot;
        }

        // a name registered twice stops the process in every build, since
        // the winner would otherwise be whichever object the linker put first
        void Register(TypeDesc& desc);
    };

    template<typename T>
    const TypeDesc* getDesc() {
        return &ClassRegistry::Get().DescFor<T>();
    }

    namespace detail
    {
        template<typename T>
            requires HasEnumTraits<T>
        std::span<const EnumeratorDesc> enumeratorsOf() {
            static constexpr auto list = [] {
                constexpr auto count = EnumTraits<T>::entries.size();

                std::array<EnumeratorDesc, count> out{};
                for(usize i = 0; i < count; ++i) {
                    out[i] = EnumeratorDesc{
                        EnumTraits<T>::entries[i].name,
                        static_cast<i64>(EnumTraits<T>::entries[i].value)
                    };
                }
                return out;
            }();

            return list;
        }

        template<typename T>
        usize listCount(const void* items) {
            return static_cast<const T*>(items)->size();
        }

        template<typename T>
        void listResize(void* items, usize count) {
            static_cast<T*>(items)->resize(count);
        }

        template<typename T>
        void* listElement(void* items, usize index) {
            return &(*static_cast<T*>(items))[index];
        }

        template<typename T>
        const void* listConstElement(const void* items, usize index) {
            return &(*static_cast<const T*>(items))[index];
        }

        template<typename T>
        TypeOps makeTypeOps() {
            static_assert(
                HasTypeTraits<T> || std::is_class_v<T>,
                "type has no TypeTraits and cannot be reflected"
            );

            TypeOps ops{
                .name = typeid(T).name(),
                .size = sizeof(T),
                .align = alignof(T)
            };

            if constexpr(std::is_default_constructible_v<T>) {
                ops.construct = [](void* at) {
                    std::construct_at(static_cast<T*>(at));
                };
            }
            if constexpr(std::is_move_constructible_v<T>) {
                ops.moveConstruct = [](void* to, void* from) {
                    std::construct_at(
                        static_cast<T*>(to),
                        std::move(*static_cast<T*>(from))
                    );
                };
            }
            if constexpr(std::is_destructible_v<T>) {
                ops.destroy = [](void* at) {
                    std::destroy_at(static_cast<T*>(at));
                };
            }

            if constexpr(HasTypeTraits<T>) {
                ops.name = TypeTraits<T>::name;
                ops.deserialize = &TypeTraits<T>::deserialize;
                ops.serialize = &TypeTraits<T>::serialize;
            }
            // the desc may stay empty, which just means
            // the type was never registered
            if constexpr(std::is_class_v<T> && !ListType<T>) {
                ops.getDesc = &getDesc<T>;
            }
            if constexpr(ListType<T>) {
                using Element = typename T::value_type;
                static_assert(
                    !std::same_as<Element, bool>,
                    "std::vector<bool> has no element addresses"
                );
                static_assert(
                    HasTypeTraits<Element> || (std::is_class_v<Element> &&
                                               !std::ranges::range<Element>),
                    "a list's element is a leaf type or a reflectable struct"
                );
                static_assert(
                    std::is_default_constructible_v<Element>,
                    "a longer list default-constructs its new elements"
                );

                static constexpr ListOps list{
                    .elementOps = &getTypeOps<Element>,
                    .count = &listCount<T>,
                    .resize = &listResize<T>,
                    .element = &listElement<T>,
                    .constElement = &listConstElement<T>
                };
                ops.name = "list";
                ops.list = &list;
            }
            if constexpr(HasEnumTraits<T>) {
                ops.enumerators = &enumeratorsOf<T>;
                ops.enumLoad = [](const void* member) {
                    return static_cast<i64>(*static_cast<const T*>(member));
                };
                ops.enumStore = [](void* member, i64 value) {
                    *static_cast<T*>(member) = static_cast<T>(value);
                };
            }

            return ops;
        }
    }

    template<typename T>
    const TypeOps* getTypeOps() {
        static const TypeOps ops = detail::makeTypeOps<T>();
        return &ops;
    }

    // the registered type this property or type recurses into,
    // or nullptr when it is a leaf or a list
    const TypeDesc* nestedDesc(const PropertyDesc&);
    const TypeDesc* nestedDesc(const TypeOps&);

    // what applyProperties could not apply, one message per key, sorted
    using PropertyErrors = std::vector<Str>;

    // parents first, then own properties in declaration order; a key the
    // table lacks keeps the member's value, and everything else that does
    // not bind is reported while the rest still applies
    [[nodiscard]] PropertyErrors applyProperties(
        const TypeDesc&,
        void* object,
        const DOM::Value& table
    );
    // the same walk in the other direction, into one flat table
    void serializeProperties(
        const TypeDesc&,
        const void* object,
        DOM::Value& out
    );

    // one value as a property of its type is applied, its errors under path
    [[nodiscard]] PropertyErrors applyValue(
        const TypeOps&,
        void* member,
        const DOM::Value& value,
        StrView path
    );
    // one value as a property of its type is written; false when the type
    // has nothing to write
    bool serializeValue(const TypeOps&, const void* member, DOM::Value& out);

    template<typename T>
        requires(!std::is_pointer_v<T>)
    [[nodiscard]] PropertyErrors applyProperties(
        T* object,
        const DOM::Value& dom
    ) {
        auto& desc = Crowy::ClassRegistry::Get().DescFor<T>();
        return applyProperties(desc, object, dom);
    }

    template<typename T>
        requires(!std::is_pointer_v<T>)
    void serializeProperties(const T* object, DOM::Value& out) {
        auto& desc = Crowy::ClassRegistry::Get().DescFor<T>();
        serializeProperties(desc, object, out);
    }

    // Member is void or const void, as the target was
    template<typename Member>
    struct BasicResolvedProperty {
        Member* member = nullptr;
        // what member is; null on failure, and error says which segment and why
        const TypeOps* type = nullptr;
        // the named property's; an element shares its list's
        const PropertyMeta* meta = nullptr;
        Str error;
    };

    using ResolvedProperty = BasicResolvedProperty<void>;
    using ResolvedConstProperty = BasicResolvedProperty<const void>;

    // "a.b[2].c" from target: each interior segment must be a reflected
    // struct, and an [index] picks one element of a list
    ResolvedProperty resolveProperty(
        void* target,
        const TypeDesc&,
        StrView path
    );
    ResolvedConstProperty resolveProperty(
        const void* target,
        const TypeDesc&,
        StrView path
    );

    template<typename Self, typename T>
    class DescBuilder {
    protected:
        TypeDesc& desc;
        PropertyDesc* lastProp = nullptr;

        explicit DescBuilder(TypeDesc& desc)
            : desc(desc) {}

        Self& self() { return static_cast<Self&>(*this); }

    public:
        Self& SetName(CStr name) {
            desc.name = name;

            return self();
        }

        template<typename Parent>
            requires std::is_base_of_v<Parent, T>
        Self& Inherits() {
            desc.parent = &ClassRegistry::Get().DescFor<Parent>();

            return self();
        }

        template<typename... Ptrs>
            requires detail::MemberChainOf<T, Ptrs...>
        Self& AddProperty(CStr name, Ptrs... ptrs) {
            lastProp = &desc.AddProperty(name, ptrs...);

            return self();
        }

        Self& SetTooltip(CStr tooltip) {
            if(lastProp != nullptr) {
                lastProp->meta.tooltip = tooltip;
            }

            return self();
        }

        Self& SetUIRange(f32 min, f32 max) {
            if(lastProp != nullptr) {
                lastProp->meta.uiRange = {min, max};
            }

            return self();
        }
    };

    template<typename T>
    class StructBuilder;

    template<typename T>
    StructBuilder<T> reflectStruct();

    template<typename T>
    class StructBuilder: public DescBuilder<StructBuilder<T>, T> {
        using Base = DescBuilder<StructBuilder<T>, T>;

    public:
        bool Build() {
            ClassRegistry::Get().Register(this->desc);

            return true;
        }

    private:
        friend StructBuilder reflectStruct<T>();

        StructBuilder()
            : Base(ClassRegistry::Get().DescFor<T>()) {}
    };

    template<typename T>
    StructBuilder<T> reflectStruct() {
        return StructBuilder<T>();
    }
}

#define CROWY_STRUCT(TYPE)                                                     \
    namespace                                                                  \
    {                                                                          \
        const auto Is##TYPE##Registered =                                      \
            ::Crowy::reflectStruct<TYPE>().SetName(#TYPE)

#define CROWY_STRUCT_END(TYPE)                                                 \
    .Build();                                                                  \
    }
