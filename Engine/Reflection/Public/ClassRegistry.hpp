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
    // Reflection target is Object
    class Object;
    using ObjectRAII = RAII<Object>;

    struct ClassDesc{
        Str name;
        const ClassDesc* parent = nullptr;
        std::function<ObjectRAII()> factory;
        StringHashMap<PropertyDesc> properties;

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

    class ClassRegistry{
    private:
        std::unordered_map<std::type_index, RAII<ClassDesc>> classByTypeindex;
        StringHashMap<ClassDesc*> classByName;

    public:
        static ObjectRAII Create(StrView type);

    private:
        template<typename T>
            requires std::is_base_of_v<Object, T>
        friend class ClassBuilder;

    public:
        static ClassRegistry& Get();

        template<typename T>
        ClassDesc& DescFor(){
            auto& slot = classByTypeindex[std::type_index(typeid(T))];

            if(slot == nullptr){
                slot = std::make_unique<ClassDesc>();
            }

            return *slot;
        }

        bool Register(ClassDesc& desc);
    };

    namespace detail
    {
        void ApplyProperties(const ClassDesc&, void* object, const DOM::Value&);
    }

    template<typename T>
        requires (!std::is_pointer_v<T>)
    void ApplyProperties(T* object, const DOM::Value& dom){
        auto& desc = Crowy::ClassRegistry::Get().DescFor<T>();
        detail::ApplyProperties(desc, object, dom);
    }

    template<typename T>
        requires std::is_base_of_v<Object, T>
    class ClassBuilder;

    template<typename T>
        requires std::is_base_of_v<Object, T>
    ClassBuilder<T> Reflect();

    template<typename T>
        requires std::is_base_of_v<Object, T>
    class ClassBuilder{
        ClassDesc& desc;
        PropertyDesc* lastProp = nullptr;

    public:
        ClassBuilder& SetName(CStr name){
            desc.name = name;

            return *this;
        }

        template<typename Parent>
        ClassBuilder& Inherits(){
            desc.parent = &ClassRegistry::Get().DescFor<Parent>();

            return *this;
        }

        ClassBuilder& SetFactory(std::function<RAII<T>()>&& f){
            desc.factory = [f = std::move(f)]() -> RAII<Object> {
                return f();
            };

            return *this;
        }

        template<typename... Ptrs>
            requires detail::MemberChainOf<T, Ptrs...>
        ClassBuilder& SetProperty(CStr name, Ptrs... ptrs){
            lastProp = &desc.AddProperty(name, ptrs...);

            return *this;
        }

        ClassBuilder& SetTooltip(CStr tooltip){
            if(lastProp != nullptr){
                lastProp->meta.tooltip = tooltip;
            }

            return *this;
        }

        bool Build(){
            auto& registry = ClassRegistry::Get();
            return registry.Register(desc);
        }

    private:
        friend ClassBuilder Reflect<T>();

        ClassBuilder()
            : desc(ClassRegistry::Get().DescFor<T>())
        {
            // TODO.
            if constexpr(std::is_default_constructible_v<T>){
                desc.factory = []() -> ObjectRAII {
                    return std::make_unique<T>();
                };
            }
        }
    };
}
