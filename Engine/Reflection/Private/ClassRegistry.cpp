#include "ClassRegistry.hpp"

#include <algorithm>
#include <format>
#include <optional>

#include "Assert.hpp"

namespace Crowy
{
    ClassRegistry& ClassRegistry::Get() {
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

    const TypeDesc* nestedDesc(const PropertyDesc& prop) {
        return nestedDesc(prop.type);
    }

    const TypeDesc* nestedDesc(const TypeOps& type) {
        if(type.getDesc == nullptr)
            return nullptr;

        const TypeDesc* desc = type.getDesc();
        return desc->HasProperties() ? desc : nullptr;
    }

    namespace
    {
        Str joinPath(StrView prefix, StrView name) {
            return prefix.empty() ? Str(name)
                                  : std::format("{}.{}", prefix, name);
        }

        Str expectedOf(const TypeOps& type) {
            if(type.enumerators == nullptr)
                return type.name;

            return std::format(
                "{} (one of {})",
                type.name,
                join(type.enumerators(), ", ", &EnumeratorDesc::name)
            );
        }

        void applyTable(
            const TypeDesc& desc,
            void* object,
            const DOM::Table& table,
            StrView prefix,
            PropertyErrors& errors
        );

        void applyNode(
            const TypeOps& type,
            void* member,
            const DOM::Value& node,
            StrView path,
            PropertyErrors& errors
        ) {
            // a reflected type is filled property by property,
            // so its unspecified members keep their default too
            if(const auto* nested = nestedDesc(type)) {
                if(const auto* inner = node.asTable()) {
                    applyTable(*nested, member, *inner, path, errors);
                }
                else {
                    errors.push_back(
                        std::format(
                            "'{}' is a struct and expects a table",
                            path
                        )
                    );
                }
            }
            else if(const auto* list = type.list) {
                const auto* items = node.asArray();
                if(items == nullptr) {
                    errors.push_back(
                        std::format("'{}' is a list and expects an array", path)
                    );

                    return;
                }

                // the file's length is the list's
                list->resize(member, items->size());
                const auto& element = *list->elementOps();
                for(usize i = 0; i < items->size(); ++i) {
                    applyNode(
                        element,
                        list->element(member, i),
                        (*items)[i],
                        std::format("{}[{}]", path, i),
                        errors
                    );
                }
            }
            else if(type.deserialize == nullptr) {
                errors.push_back(
                    std::format(
                        "'{}' is a {} with no properties registered",
                        path,
                        type.name
                    )
                );
            }
            else if(!type.deserialize(member, node)) {
                errors.push_back(
                    std::format("'{}' expects {}", path, expectedOf(type))
                );
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

            for(const auto* prop: desc.PropertiesInChain()) {
                const auto node = table.find(prop->name);
                if(node == table.end()) {
                    continue;
                }

                applyNode(
                    prop->type,
                    prop->accessor->Get(object),
                    node->second,
                    joinPath(prefix, prop->name),
                    errors
                );
            }
        }
    }

    PropertyErrors applyProperties(
        const TypeDesc& desc,
        void* object,
        const DOM::Value& table
    ) {
        PropertyErrors errors;

        if(const auto* root = table.asTable()) {
            applyTable(desc, object, *root, {}, errors);
        }
        else {
            errors.push_back(std::format("'{}' expects a table", desc.name));
        }

        // sorted, so the report does not depend on the file's key order
        std::ranges::sort(errors);

        return errors;
    }

    PropertyErrors applyValue(
        const TypeOps& type,
        void* member,
        const DOM::Value& value,
        StrView path
    ) {
        PropertyErrors errors;
        applyNode(type, member, value, path, errors);
        std::ranges::sort(errors);

        return errors;
    }

    bool serializeValue(
        const TypeOps& type,
        const void* member,
        DOM::Value& out
    ) {
        if(const auto* nested = nestedDesc(type)) {
            serializeProperties(*nested, member, out);

            return true;
        }
        if(const auto* list = type.list) {
            const auto& element = *list->elementOps();
            const auto count = list->count(member);

            DOM::Array items;
            items.reserve(count);
            for(usize i = 0; i < count; ++i) {
                // an element with nothing to write is null, so the
                // indices still line up
                DOM::Value item;
                serializeValue(element, list->constElement(member, i), item);
                items.push_back(std::move(item));
            }
            out = DOM::Value(std::move(items));

            return true;
        }
        if(type.serialize != nullptr) {
            type.serialize(member, out);

            return true;
        }

        return false;
    }

    void serializeProperties(
        const TypeDesc& desc,
        const void* object,
        DOM::Value& out
    ) {
        DOM::Table table;
        for(const auto* prop: desc.PropertiesInChain()) {
            DOM::Value value;
            if(serializeValue(prop->type, prop->accessor->Get(object), value))
                table.emplace(prop->name, std::move(value));
        }
        out = DOM::Value(std::move(table));
    }

    namespace
    {
        std::optional<usize> parseIndex(StrView text) {
            if(text.size() < 3 || text.front() != '[' || text.back() != ']')
                return std::nullopt;

            return parseNumber<usize>(text.substr(1, text.size() - 2));
        }

        template<typename Member>
        BasicResolvedProperty<Member> resolveIn(
            Member* target,
            const TypeDesc& desc,
            StrView path
        ) {
            BasicResolvedProperty<Member> result;

            auto* object = target;
            const TypeDesc* current = &desc;
            StrView rest = path;
            while(true) {
                const auto dot = rest.find('.');
                const auto segment = rest.substr(0, dot);
                const auto bracket = segment.find('[');
                const auto name = segment.substr(0, bracket);
                if(name.empty()) {
                    result.error = std::format("empty segment in '{}'", path);
                    return result;
                }

                const auto* prop = current->FindInChain(name);
                if(prop == nullptr) {
                    result.error = std::format(
                        "no property '{}' on '{}'",
                        name,
                        current->name
                    );
                    return result;
                }

                auto* member = prop->accessor->Get(object);
                const auto* type = &prop->type;
                if(bracket != StrView::npos) {
                    const auto index = parseIndex(segment.substr(bracket));
                    if(!index) {
                        result.error = std::format("bad index in '{}'", path);
                        return result;
                    }

                    const auto* list = type->list;
                    if(list == nullptr) {
                        result.error = std::format("'{}' is not a list", name);
                        return result;
                    }

                    const auto count = list->count(member);
                    if(*index >= count) {
                        result.error = std::format(
                            "'{}' is out of range ({} elements)",
                            segment,
                            count
                        );
                        return result;
                    }

                    if constexpr(std::is_const_v<Member>) {
                        member = list->constElement(member, *index);
                    }
                    else {
                        member = list->element(member, *index);
                    }
                    type = list->elementOps();
                }

                if(dot == StrView::npos) {
                    result.member = member;
                    result.type = type;
                    result.meta = &prop->meta;
                    return result;
                }

                const auto* nested = nestedDesc(*type);
                if(nested == nullptr) {
                    result.error =
                        std::format("'{}' has no properties", segment);
                    return result;
                }

                object = member;
                current = nested;
                rest.remove_prefix(dot + 1);
            }
        }
    }

    ResolvedProperty resolveProperty(
        void* target,
        const TypeDesc& desc,
        StrView path
    ) {
        return resolveIn(target, desc, path);
    }

    ResolvedConstProperty resolveProperty(
        const void* target,
        const TypeDesc& desc,
        StrView path
    ) {
        return resolveIn(target, desc, path);
    }
}
