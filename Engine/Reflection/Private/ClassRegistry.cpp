#include "ClassRegistry.hpp"

#include <algorithm>
#include <format>

#include "Assert.hpp"
#include "Object.hpp"

namespace Crowy
{
    ObjectRAII ClassRegistry::Create(StrView type){
        const auto* desc = dynamic_cast<const ClassDesc*>(FindType(type));

        return desc != nullptr ? desc->factory() : nullptr;
    }

    ClassRegistry& ClassRegistry::Get(){
        static ClassRegistry singleton;
        return singleton;
    }

    const TypeDesc* ClassRegistry::FindType(StrView name) {
        const auto& typeByName = Get().typeByName;
        const auto it = typeByName.find(name);

        return it == typeByName.end() ? nullptr : it->second;
    }

    void ClassRegistry::Register(TypeDesc& desc) {
        CROWY_ASSERT(!desc.name.empty());

        const auto [it, inserted] = typeByName.try_emplace(desc.name, &desc);
        if(!inserted) {
            detail::assertFail(
                "inserted",
                std::source_location::current(),
                it->second == &desc
                    ? std::format("'{}' is registered twice", desc.name)
                    : std::format("two types are registered as '{}'", desc.name)
            );
        }
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

    namespace
    {
        Str joinPath(StrView prefix, StrView name) {
            return prefix.empty() ? Str(name)
                                  : std::format("{}.{}", prefix, name);
        }

        void applyTable(
            const TypeDesc& desc,
            void* object,
            const DOM::Table& table,
            StrView prefix,
            PropertyErrors& errors
        );

        void applyOwnProperties(
            const TypeDesc& desc,
            void* object,
            const DOM::Table& table,
            StrView prefix,
            PropertyErrors& errors
        ) {
            if(desc.parent != nullptr) {
                applyOwnProperties(*desc.parent, object, table, prefix, errors);
            }

            for(const auto& prop: desc.properties) {
                const auto node = table.find(prop.name);
                // an absent key keeps the member's value
                if(node == table.end()) {
                    continue;
                }

                auto member = prop.accessor->Get(object);
                const auto path = joinPath(prefix, prop.name);

                // a reflected type is filled property by property,
                // so its unspecified members keep their default too
                if(const auto* nested = NestedDesc(prop)) {
                    if(const auto* inner = node->second.asTable()) {
                        applyTable(*nested, member, *inner, path, errors);
                    } else {
                        errors.push_back(
                            std::format(
                                "'{}' is a struct and expects a table",
                                path
                            )
                        );
                    }
                } else if(prop.type.deserialize == nullptr) {
                    errors.push_back(
                        std::format(
                            "'{}' is a {} with no properties registered",
                            path,
                            prop.type.name
                        )
                    );
                } else if(!prop.type.deserialize(member, node->second)) {
                    errors.push_back(
                        std::format("'{}' expects {}", path, prop.type.name)
                    );
                }
            }
        }

        // the unknown-key check runs once per table, against the whole chain,
        // since a parent's keys sit in the child's table
        void applyTable(
            const TypeDesc& desc,
            void* object,
            const DOM::Table& table,
            StrView prefix,
            PropertyErrors& errors
        ) {
            for(const auto& [key, value]: table) {
                if(desc.FindInChain(key) == nullptr) {
                    errors.push_back(
                        std::format(
                            "no property '{}' on '{}'",
                            joinPath(prefix, key),
                            desc.name
                        )
                    );
                }
            }

            applyOwnProperties(desc, object, table, prefix, errors);
        }
    }

    PropertyErrors ApplyProperties(
        const TypeDesc& desc,
        void* object,
        const DOM::Value& table
    ) {
        PropertyErrors errors;

        if(const auto* root = table.asTable()) {
            applyTable(desc, object, *root, {}, errors);
        } else {
            errors.push_back(std::format("'{}' expects a table", desc.name));
        }

        // the table is unordered, so the report is sorted to stay stable
        std::ranges::sort(errors);

        return errors;
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
