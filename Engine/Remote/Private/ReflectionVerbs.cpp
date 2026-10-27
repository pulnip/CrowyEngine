#include "ReflectionVerbs.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <utility>
#include <vector>

#include "Assert.hpp"
#include "StringUtil.hpp"

namespace Crowy
{
    namespace
    {
        const Exposure* findTarget(
            const Exposures& exposures,
            const DOM::Value& args,
            Reply& reply
        ) {
            const auto name = args.get<Str>("target");
            if(!name) {
                reply.Error("\"target\" is missing or not a string");

                return nullptr;
            }

            const auto it = exposures.find(*name);
            if(it == exposures.end()) {
                reply.Error(std::format("unknown target '{}'", *name));

                return nullptr;
            }

            return &it->second;
        }

        template<typename Object>
        Object* present(Object* object, const DOM::Value& args, Reply& reply) {
            if(object == nullptr) {
                reply.Error(
                    std::format("target '{}' is gone", *args.get<Str>("target"))
                );
            }

            return object;
        }

        DOM::Value describeProperties(const TypeDesc& desc);

        void describeType(const TypeOps& type, DOM::Table& entry) {
            const auto* nested = nestedDesc(type);
            // a struct's TypeOps name is the compiler's; the desc has the
            // registered one
            entry.emplace(
                "type",
                nested != nullptr ? nested->name : Str(type.name)
            );

            if(type.enumerators != nullptr) {
                DOM::Array names;
                for(const auto& enumerator: type.enumerators()) {
                    names.emplace_back(enumerator.name);
                }

                entry.emplace("enumerators", std::move(names));
            }

            if(type.list != nullptr) {
                DOM::Table element;
                describeType(*type.list->elementOps(), element);

                entry.emplace("element", std::move(element));
            }

            if(nested != nullptr) {
                entry.emplace("properties", describeProperties(*nested));
            }
        }

        void appendDescriptions(const TypeDesc& desc, DOM::Array& out) {
            if(desc.parent != nullptr) {
                appendDescriptions(*desc.parent, out);
            }

            for(const auto& prop: desc.properties) {
                DOM::Table entry;
                entry.emplace("name", prop.name);
                describeType(prop.type, entry);

                if(prop.meta.uiRange) {
                    DOM::Array range;
                    range.emplace_back(prop.meta.uiRange->first);
                    range.emplace_back(prop.meta.uiRange->second);

                    entry.emplace("uiRange", std::move(range));
                }

                if(prop.meta.tooltip != nullptr) {
                    entry.emplace("tooltip", prop.meta.tooltip);
                }

                out.emplace_back(std::move(entry));
            }
        }

        DOM::Value describeProperties(const TypeDesc& desc) {
            DOM::Array properties;
            appendDescriptions(desc, properties);

            return DOM::Value(std::move(properties));
        }

        PropertyErrors tryValue(
            const TypeOps& type,
            const DOM::Value& value,
            StrView path
        ) {
            CROWY_ASSERT(type.align <= alignof(std::max_align_t));
            std::vector<std::max_align_t> storage(
                type.size / sizeof(std::max_align_t) + 1
            );
            auto* scratch = storage.data();

            type.construct(scratch);
            auto errors = applyValue(type, scratch, value, path);
            type.destroy(scratch);

            return errors;
        }
    }

    void listObjects(
        const Exposures& exposures,
        const DOM::Value&,
        Reply reply
    ) {
        std::vector<std::pair<StrView, const Exposure*>> sorted;
        sorted.reserve(exposures.size());

        for(const auto& [name, exposure]: exposures) {
            sorted.emplace_back(name, &exposure);
        }
        std::ranges::sort(
            sorted,
            {},
            &std::pair<StrView, const Exposure*>::first
        );

        DOM::Array objects;
        objects.reserve(sorted.size());

        for(const auto& [name, exposure]: sorted) {
            DOM::Table entry;
            entry.emplace("name", Str(name));
            entry.emplace("type", exposure->target.Desc().name);

            objects.emplace_back(std::move(entry));
        }

        DOM::Table result;
        result.emplace("objects", std::move(objects));

        reply.Ok(std::move(result));
    }

    void describeObject(
        const Exposures& exposures,
        const DOM::Value& args,
        Reply reply
    ) {
        const auto* exposure = findTarget(exposures, args, reply);
        if(exposure == nullptr)
            return;

        const auto& desc = exposure->target.Desc();

        DOM::Table result;
        result.emplace("type", desc.name);
        result.emplace("properties", describeProperties(desc));

        reply.Ok(std::move(result));
    }

    void getProperty(
        const Exposures& exposures,
        const DOM::Value& args,
        Reply reply
    ) {
        const auto* exposure = findTarget(exposures, args, reply);
        if(exposure == nullptr)
            return;

        const auto object = present(exposure->target.Read(), args, reply);
        if(object == nullptr)
            return;
        const auto& desc = exposure->target.Desc();

        DOM::Value value;
        if(const auto* node = args.at("path")) {
            const auto* path = node->asString();
            if(path == nullptr) {
                reply.Error("\"path\" is not a string");

                return;
            }

            auto resolved = resolveProperty(object, desc, *path);
            if(resolved.type == nullptr) {
                reply.Error(std::move(resolved.error));

                return;
            }

            const auto& type = *resolved.type;
            if(!serializeValue(type, resolved.member, value)) {
                reply.Error(
                    std::format(
                        "'{}' is a {} the port cannot read",
                        *path,
                        type.name
                    )
                );

                return;
            }
        }
        else {
            serializeProperties(desc, object, value);
        }

        DOM::Table result;
        result.emplace("value", std::move(value));

        reply.Ok(std::move(result));
    }

    void setProperty(
        const Exposures& exposures,
        const DOM::Value& args,
        Reply reply
    ) {
        const auto* exposure = findTarget(exposures, args, reply);
        if(exposure == nullptr)
            return;

        const auto path = args.get<Str>("path");
        if(!path) {
            reply.Error("\"path\" is missing or not a string");

            return;
        }

        const auto* value = args.at("value");
        if(value == nullptr) {
            reply.Error("\"value\" is missing");

            return;
        }

        const auto readable = present(exposure->target.Read(), args, reply);
        if(readable == nullptr)
            return;
        const auto& desc = exposure->target.Desc();

        const auto checked = resolveProperty(readable, desc, *path);
        if(checked.type == nullptr) {
            reply.Error(checked.error);

            return;
        }

        // a whole-struct write would go through applyProperties, which writes
        // what binds before it reports the rest; the port answers per leaf
        if(nestedDesc(*checked.type) != nullptr) {
            reply.Error(
                std::format(
                    "'{}' is a struct; set one of its properties",
                    *path
                )
            );

            return;
        }

        // a list is written whole, as the inspector's add and remove do
        if(checked.type->list != nullptr) {
            const auto errors = tryValue(*checked.type, *value, *path);
            if(!errors.empty()) {
                reply.Error(join(errors, "; "));

                return;
            }

            const auto writable =
                present(exposure->target.Write(), args, reply);
            if(writable == nullptr)
                return;

            const auto resolved = resolveProperty(writable, desc, *path);
            const auto written =
                applyValue(*resolved.type, resolved.member, *value, *path);
            CROWY_ASSERT(written.empty(), "the scratch write passed");

            if(exposure->onDirty) {
                exposure->onDirty();
            }

            reply.Ok(DOM::Table{});

            return;
        }

        if(checked.type->deserialize == nullptr) {
            reply.Error(
                std::format(
                    "'{}' is a {} the port cannot write",
                    *path,
                    checked.type->name
                )
            );

            return;
        }

        const auto writable = present(exposure->target.Write(), args, reply);
        if(writable == nullptr)
            return;

        const auto resolved = resolveProperty(writable, desc, *path);
        if(!resolved.type->deserialize(resolved.member, *value)) {
            reply.Error(
                std::format("'{}' expects {}", *path, resolved.type->name)
            );

            return;
        }

        if(exposure->onDirty) {
            exposure->onDirty();
        }

        reply.Ok(DOM::Table{});
    }
}
