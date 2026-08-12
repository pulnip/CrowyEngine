#include "Assert.hpp"
#include "ClassRegistry.hpp"
#include "Object.hpp"

namespace Crowy
{
    ObjectRAII ClassRegistry::Create(StrView type){
        static auto& registry = ClassRegistry::Get();
        static const auto& classByName = registry.classByName;

        auto it = classByName.find(type);
        if(it == classByName.end()){
            return nullptr;
        }

        const auto& desc = *it->second;
        return desc.factory();
    }

    ClassRegistry& ClassRegistry::Get(){
        static ClassRegistry singleton;
        return singleton;
    }

    bool ClassRegistry::Register(ClassDesc& desc){
        CROWY_ASSERT(!desc.name.empty());
        CROWY_ASSERT(desc.factory);
        auto [_, ret] = classByName.try_emplace(desc.name, &desc);

        return ret;
    }

    namespace detail
    {
        static bool HasProperties(const TypeDesc& desc){
            if(!desc.properties.empty()){
                return true;
            }

            return desc.parent != nullptr && HasProperties(*desc.parent);
        }

        // a registered type, or nullptr when the property is a leaf
        static const TypeDesc* NestedDesc(const PropertyDesc& prop){
            if(prop.typeInfo.getDesc == nullptr){
                return nullptr;
            }

            const TypeDesc* desc = prop.typeInfo.getDesc();
            return HasProperties(*desc) ? desc : nullptr;
        }

        void ApplyProperties(const TypeDesc& desc, void* object, const DOM::Value& table){
            if(desc.parent != nullptr){
                ApplyProperties(*desc.parent, object, table);
            }

            for(const auto& [name, prop]: desc.properties){
                auto node = table.at(name);

                // use default value if prop is not specified
                if(node == nullptr){
                    continue;
                }

                auto member = prop.accessor->Get(object);

                // a reflected type is filled property by property,
                // so its unspecified members keep their default too
                auto nested = NestedDesc(prop);
                if(nested != nullptr && node->is_table()){
                    ApplyProperties(*nested, member, *node);
                }
                else if(prop.typeInfo.deserialize != nullptr){
                    prop.typeInfo.deserialize(member, *node);
                }
            }
        }
    }
}
