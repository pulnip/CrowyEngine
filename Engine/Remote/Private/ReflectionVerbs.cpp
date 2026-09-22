#include "ReflectionVerbs.hpp"

#include <algorithm>
#include <format>
#include <utility>
#include <vector>

namespace Crowy
{
    namespace
    {
        // answers the reply itself when the target cannot be found
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

        DOM::Value describeProperties(const TypeDesc& desc);

        // parents first, then own properties in declaration order,
        // with the metadata a generic client needs to build a control
        void appendDescriptions(const TypeDesc& desc, DOM::Array& out) {
            if(desc.parent != nullptr) {
                appendDescriptions(*desc.parent, out);
            }

            for(const auto& prop: desc.properties) {
                const auto* nested = NestedDesc(prop);

                DOM::Table entry;
                entry.emplace("name", prop.name);
                // a struct's TypeOps name is the compiler's; the desc has the
                // registered one
                entry.emplace(
                    "type",
                    nested != nullptr ? nested->name : Str(prop.type.name)
                );

                if(prop.meta.uiRange) {
                    DOM::Array range;
                    range.emplace_back(prop.meta.uiRange->first);
                    range.emplace_back(prop.meta.uiRange->second);

                    entry.emplace("uiRange", std::move(range));
                }

                if(prop.meta.tooltip != nullptr) {
                    entry.emplace("tooltip", prop.meta.tooltip);
                }

                if(prop.type.enumerators != nullptr) {
                    DOM::Array names;
                    for(const auto& enumerator: prop.type.enumerators()) {
                        names.emplace_back(enumerator.name);
                    }

                    entry.emplace("enumerators", std::move(names));
                }

                if(nested != nullptr) {
                    entry.emplace("properties", describeProperties(*nested));
                }

                out.emplace_back(std::move(entry));
            }
        }

        DOM::Value describeProperties(const TypeDesc& desc) {
            DOM::Array properties;
            appendDescriptions(desc, properties);

            return DOM::Value(std::move(properties));
        }
    }

    void listObjects(
        const Exposures& exposures,
        const DOM::Value&,
        Reply reply
    ) {
        // the map has no order; the wire does
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
            entry.emplace("type", exposure->desc->name);

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

        DOM::Table result;
        result.emplace("type", exposure->desc->name);
        result.emplace("properties", describeProperties(*exposure->desc));

        reply.Ok(std::move(result));
    }
}
