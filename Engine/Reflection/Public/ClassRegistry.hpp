#pragma once

#include <functional>
#include <typeindex>
#include <unordered_map>
#include "Assert.hpp"
#include "Primitives.hpp"
#include "PropertyDesc.hpp"
#include "StringUtil.hpp"
#include "TypeInfo.hpp"

namespace Crowy
{
    // Reflection target is either a Class, an Object derived type
    // created by name, or a Struct, plain data reached as a property
    class Object;
    using ObjectRAII = RAII<Object>;

    template<typename T>
    const TypeInfo* GetTypeInfo();

    // name, inheritance and properties are common to both
    struct TypeDesc{
        Str name;
        const TypeDesc* parent = nullptr;
        StringHashMap<PropertyDesc> properties;

        // owned as RAII<TypeDesc>, so the destructor must be virtual
        virtual ~TypeDesc() = default;

        template<typename... Ptrs>
            requires detail::MemberPointers<Ptrs...>
        PropertyDesc& AddProperty(CStr name, Ptrs... ptrs){
            using Leaf = typename detail::MemberChain<Ptrs...>::Leaf;

            auto [it, ret] = properties.try_emplace(
                name,
                PropertyDesc{
                    .typeInfo = *GetTypeInfo<Leaf>(),
                    .accessor = std::make_unique<MemberAccessor<Ptrs...>>(ptrs...),
                    .meta = {}
                }
            );

            CROWY_ASSERT(ret);
            return it->second;
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
        TypeInfo MakeTypeInfo(){
            static_assert(
                HasTypeTraits<T> || std::is_class_v<T>,
                "type has no TypeTraits and cannot be reflected"
            );

            TypeInfo info{
                .name = typeid(T).name(),
                .size = sizeof(T)
            };

            if constexpr(HasTypeTraits<T>){
                info.name = TypeTraits<T>::name;
                info.deserialize = &TypeTraits<T>::deserialize;
            }
            // the desc may stay empty, which just means
            // the type was never registered
            if constexpr(std::is_class_v<T>){
                info.getDesc = &GetDesc<T>;
            }

            return info;
        }
    }

    template<typename T>
    const TypeInfo* GetTypeInfo(){
        static const TypeInfo info = detail::MakeTypeInfo<T>();
        return &info;
    }

    namespace detail
    {
        void ApplyProperties(const TypeDesc&, void* object, const DOM::Value&);
    }

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
