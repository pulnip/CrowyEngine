#pragma once

#include <array>
#include <functional>
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
    // Reflection target is either a Class, an Object derived type
    // created by name, or a Struct, plain data reached as a property
    class Object;
    using ObjectRAII = RAII<Object>;

    template<typename T>
    const TypeOps* GetTypeOps();

    // name, inheritance and properties are common to both
    struct TypeDesc{
        Str name;
        const TypeDesc* parent = nullptr;
        // registration order, which consumers treat as declaration order
        std::vector<PropertyDesc> properties;
        StringHashMap<usize> indexByName;

        // owned as RAII<TypeDesc>, so the destructor must be virtual
        virtual ~TypeDesc() = default;

        template<typename... Ptrs>
            requires detail::MemberPointers<Ptrs...>
        PropertyDesc& AddProperty(CStr name, Ptrs... ptrs){
            using Leaf = typename detail::MemberChain<Ptrs...>::Leaf;

            static_assert(
                HasTypeTraits<Leaf> ||
                    (std::is_class_v<Leaf> && !std::ranges::range<Leaf>),
                "container properties are not supported; "
                "register a leaf type or a reflectable struct"
            );

            auto [it, ret] = indexByName.try_emplace(name, properties.size());
            CROWY_ASSERT(ret);

            properties.push_back(PropertyDesc{
                .name = name,
                .type = *GetTypeOps<Leaf>(),
                .accessor = std::make_unique<MemberAccessor<Ptrs...>>(ptrs...),
                .meta = {}
            });

            return properties.back();
        }

        const PropertyDesc* Find(StrView name) const{
            auto it = indexByName.find(name);
            return it == indexByName.end() ? nullptr : &properties[it->second];
        }
    };

    struct StructDesc: public TypeDesc{};

    struct ClassDesc: public TypeDesc{
        std::function<ObjectRAII()> factory;
    };

    template<typename T>
    using DescOf = std::conditional_t<
        std::is_base_of_v<Object, T>,
        ClassDesc,
        StructDesc
    >;

    class ClassRegistry{
    private:
        std::unordered_map<std::type_index, RAII<TypeDesc>> descByTypeindex;
        // only a Class can be created by name
        StringHashMap<ClassDesc*> classByName;

    public:
        static ObjectRAII Create(StrView type);
        template<std::derived_from<Object> T>
        static ObjectRAII Create(){
            return Create(T::StaticClassName());
        }

        static ClassRegistry& Get();

        // creates the slot on demand, and its address stays valid.
        // T alone decides the slot kind, so the cast is safe
        template<typename T>
        DescOf<T>& DescFor(){
            auto& slot = descByTypeindex[std::type_index(typeid(T))];

            if(slot == nullptr){
                slot = std::make_unique<DescOf<T>>();
            }

            return static_cast<DescOf<T>&>(*slot);
        }

        bool Register(ClassDesc& desc);
    };

    template<typename T>
    const TypeDesc* GetDesc(){
        return &ClassRegistry::Get().DescFor<T>();
    }

    namespace detail
    {
        template<typename T>
            requires HasEnumTraits<T>
        std::span<const EnumeratorDesc> EnumeratorsOf(){
            static constexpr auto list = []{
                constexpr auto count = EnumTraits<T>::entries.size();

                std::array<EnumeratorDesc, count> out{};
                for(usize i = 0; i < count; ++i){
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
        TypeOps MakeTypeOps(){
            static_assert(
                HasTypeTraits<T> || std::is_class_v<T>,
                "type has no TypeTraits and cannot be reflected"
            );

            TypeOps ops{
                .name = typeid(T).name(),
                .size = sizeof(T)
            };

            if constexpr(HasTypeTraits<T>){
                ops.name = TypeTraits<T>::name;
                ops.deserialize = &TypeTraits<T>::deserialize;
            }
            // the desc may stay empty, which just means
            // the type was never registered
            if constexpr(std::is_class_v<T>){
                ops.getDesc = &GetDesc<T>;
            }
            if constexpr(HasEnumTraits<T>){
                ops.enumerators = &EnumeratorsOf<T>;
            }

            return ops;
        }
    }

    template<typename T>
    const TypeOps* GetTypeOps(){
        static const TypeOps ops = detail::MakeTypeOps<T>();
        return &ops;
    }

    namespace detail
    {
        void ApplyProperties(const TypeDesc&, void* object, const DOM::Value&);
    }

    // the registered type this property recurses into,
    // or nullptr when the property is a leaf
    const TypeDesc* NestedDesc(const PropertyDesc&);

    template<typename T>
        requires (!std::is_pointer_v<T>)
    void ApplyProperties(T* object, const DOM::Value& dom){
        auto& desc = Crowy::ClassRegistry::Get().DescFor<T>();
        detail::ApplyProperties(desc, object, dom);
    }

    // everything a Class and a Struct declare the same way
    template<typename Self, typename T, typename Desc>
    class DescBuilder{
    protected:
        Desc& desc;
        PropertyDesc* lastProp = nullptr;

        explicit DescBuilder(Desc& desc)
            : desc(desc){}

        Self& self(){
            return static_cast<Self&>(*this);
        }

    public:
        Self& SetName(CStr name){
            desc.name = name;

            return self();
        }

        template<typename Parent>
            requires std::is_base_of_v<Parent, T>
        Self& Inherits(){
            desc.parent = &ClassRegistry::Get().DescFor<Parent>();

            return self();
        }

        template<typename... Ptrs>
            requires detail::MemberChainOf<T, Ptrs...>
        Self& SetProperty(CStr name, Ptrs... ptrs){
            lastProp = &desc.AddProperty(name, ptrs...);

            return self();
        }

        Self& SetTooltip(CStr tooltip){
            if(lastProp != nullptr){
                lastProp->meta.tooltip = tooltip;
            }

            return self();
        }

        Self& SetUIRange(f32 min, f32 max){
            if(lastProp != nullptr){
                lastProp->meta.uiRange = {min, max};
            }

            return self();
        }
    };

    template<typename T>
        requires std::is_base_of_v<Object, T>
    class ClassBuilder;

    template<typename T>
        requires std::is_base_of_v<Object, T>
    ClassBuilder<T> Reflect();

    template<typename T>
        requires std::is_base_of_v<Object, T>
    class ClassBuilder: public DescBuilder<ClassBuilder<T>, T, ClassDesc>{
        using Base = DescBuilder<ClassBuilder<T>, T, ClassDesc>;

    public:
        ClassBuilder& SetFactory(std::function<RAII<T>()>&& f){
            this->desc.factory = [f = std::move(f)]() -> ObjectRAII {
                return f();
            };

            return *this;
        }

        bool Build(){
            auto& registry = ClassRegistry::Get();
            return registry.Register(this->desc);
        }

    private:
        friend ClassBuilder Reflect<T>();

        ClassBuilder()
            : Base(ClassRegistry::Get().DescFor<T>())
        {
            // TODO.
            if constexpr(std::is_default_constructible_v<T>){
                this->desc.factory = []() -> ObjectRAII {
                    return std::make_unique<T>();
                };
            }
        }
    };

    template<typename T>
        requires (!std::is_base_of_v<Object, T>)
    class StructBuilder;

    template<typename T>
        requires (!std::is_base_of_v<Object, T>)
    StructBuilder<T> ReflectStruct();

    template<typename T>
        requires (!std::is_base_of_v<Object, T>)
    class StructBuilder: public DescBuilder<StructBuilder<T>, T, StructDesc>{
        using Base = DescBuilder<StructBuilder<T>, T, StructDesc>;

    public:
        bool Build(){
            // a Struct has no name lookup, its desc is already in place
            CROWY_ASSERT(!this->desc.name.empty());

            return true;
        }

    private:
        friend StructBuilder ReflectStruct<T>();

        StructBuilder()
            : Base(ClassRegistry::Get().DescFor<T>()){}
    };
}
