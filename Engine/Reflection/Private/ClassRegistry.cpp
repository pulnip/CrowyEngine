#include <format>
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

    namespace
    {
        bool HasProperties(const TypeDesc& desc){
            if(!desc.properties.empty()){
                return true;
            }

            return desc.parent != nullptr && HasProperties(*desc.parent);
        }
    }

    const TypeDesc* NestedDesc(const PropertyDesc& prop){
        if(prop.type.getDesc == nullptr){
            return nullptr;
        }

        const TypeDesc* desc = prop.type.getDesc();
        return HasProperties(*desc) ? desc : nullptr;
    }

    void ApplyProperties(const TypeDesc& desc, void* object, const DOM::Value& table){
        if(desc.parent != nullptr){
            ApplyProperties(*desc.parent, object, table);
        }

        for(const auto& prop: desc.properties){
            auto node = table.at(prop.name);

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
            else if(prop.type.deserialize != nullptr){
                prop.type.deserialize(member, *node);
            }
        }
    }

    namespace
    {
        void appendProperties(const TypeDesc& desc, const void* object, DOM::Table& table){
            if(desc.parent != nullptr){
                appendProperties(*desc.parent, object, table);
            }

            for(const auto& prop: desc.properties){
                // the accessor has one signature for both directions; this walk only reads
                const void* member = prop.accessor->Get(const_cast<void*>(object));

                DOM::Value value;
                if(const auto* nested = NestedDesc(prop)){
                    SerializeProperties(*nested, member, value);
                }
                else if(prop.type.serialize != nullptr){
                    prop.type.serialize(member, value);
                }
                else{
                    continue;
                }

                // the most derived declaration wins a shadowed name
                table.insert_or_assign(prop.name, std::move(value));
            }
        }
    }

    void SerializeProperties(const TypeDesc& desc, const void* object, DOM::Value& out){
        DOM::Table table;
        appendProperties(desc, object, table);
        out = DOM::Value(std::move(table));
    }

    ResolvedProperty ResolveProperty(void* target, const TypeDesc& desc, StrView path){
        ResolvedProperty result;

        if(path.find('[') != StrView::npos){
            result.error = std::format("index paths are not supported: '{}'", path);
            return result;
        }

        void* object = target;
        const TypeDesc* current = &desc;
        StrView rest = path;
        while(true){
            const auto dot = rest.find('.');
            const auto segment = rest.substr(0, dot);
            if(segment.empty()){
                result.error = std::format("empty segment in '{}'", path);
                return result;
            }

            const auto* prop = current->FindInChain(segment);
            if(prop == nullptr){
                result.error = std::format("no property '{}' on '{}'", segment, current->name);
                return result;
            }

            void* member = prop->accessor->Get(object);
            if(dot == StrView::npos){
                result.member = member;
                result.desc = prop;
                return result;
            }

            const auto* nested = NestedDesc(*prop);
            if(nested == nullptr){
                result.error = std::format("'{}' has no properties", segment);
                return result;
            }

            object = member;
            current = nested;
            rest.remove_prefix(dot + 1);
        }
    }
}
