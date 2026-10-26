#include "ClassRegistry.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <optional>

#include "Assert.hpp"

namespace Crowy
{
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

    const TypeDesc* nestedDesc(const PropertyDesc& prop){
        return nestedDesc(prop.type);
    }

    const TypeDesc* nestedDesc(const TypeOps& type) {
        if(type.getDesc == nullptr)
            return nullptr;

        const TypeDesc* desc = type.getDesc();
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
                } else {
                    errors.push_back(
                        std::format(
                            "'{}' is a struct and expects a table",
                            path
                        )
                    );
                }
            } else if(const auto* list = type.list) {
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
            } else if(type.deserialize == nullptr) {
                errors.push_back(
                    std::format(
                        "'{}' is a {} with no properties registered",
                        path,
                        type.name
                    )
                );
            } else if(!type.deserialize(member, node)) {
                errors.push_back(
                    std::format("'{}' expects {}", path, type.name)
                );
            }
        }

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

                applyNode(
                    prop.type,
                    prop.accessor->Get(object),
                    node->second,
                    joinPath(prefix, prop.name),
                    errors
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

            applyOwnProperties(desc, object, table, prefix, errors);
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
        } else {
            errors.push_back(std::format("'{}' expects a table", desc.name));
        }

        // sorted, so the report does not depend on the file's key order
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
                const void* member = prop.accessor->Get(object);
                DOM::Value value;
                if(!serializeValue(prop.type, member, value))
                    continue;

                // the most derived declaration wins a shadowed name
                table.insert_or_assign(prop.name, std::move(value));
            }
        }
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

    void serializeProperties(const TypeDesc& desc, const void* object, DOM::Value& out){
        DOM::Table table;
        appendProperties(desc, object, table);
        out = DOM::Value(std::move(table));
    }

    namespace
    {
        // "[N]" and nothing else
        std::optional<usize> parseIndex(StrView text) {
            if(text.size() < 3 || text.front() != '[' || text.back() != ']')
                return std::nullopt;

            usize index = 0;
            const auto* first = text.data() + 1;
            const auto* last = text.data() + text.size() - 1;
            const auto [end, error] = std::from_chars(first, last, index);
            if(error != std::errc{} || end != last)
                return std::nullopt;

            return index;
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
