#include <array>
#include <format>

#include "DomTraits.hpp"
#include "RangeUtil.hpp"
#include "StringUtil.hpp"

namespace Crowy
{
    std::vector<Str> checkDocMetadata(
        const DOM::Value& root,
        StrView type,
        i64 version
    ) {
        if(root.asTable() == nullptr)
            return {std::format("a {} is a table", type)};

        // another format altogether: nothing below would read right
        const auto givenType = root.get<Str>("metadata.type");
        if(givenType != type) {
            return {std::format(
                "expected a \"{}\" document, got \"{}\"",
                type,
                givenType.value_or("")
            )};
        }
        const auto givenVersion = root.get<i64>("metadata.version");
        if(givenVersion != version) {
            return {std::format(
                "a {} of version {} cannot be read; this reads version {}",
                type,
                givenVersion.value_or(0),
                version
            )};
        }

        constexpr std::array<StrView, 3> keys{"version", "type", "name"};
        // the type was found, so the metadata is a table
        const auto& metadata =
            *root.asTable()->find("metadata")->second.asTable();
        std::vector<Str> errors;
        for(const auto& key: stray(metadata.keys(), keys)) {
            errors.push_back(
                std::format(
                    "the {}'s metadata: unknown key '{}'; the keys are {}",
                    type,
                    key,
                    join(keys, ", ")
                )
            );
        }

        return errors;
    }

    DocMetadata DomTraits<DocMetadata>::from(const DOM::Value& root) {
        auto version = root.get<u32>("metadata.version").value_or(0);
        auto type = root.get<Str>("metadata.type").value_or("Unknown");
        auto name = root.get<Str>("metadata.name").value_or("Unnamed");

        return DocMetadata{
            .version = version,
            .type = type,
            .name = name,
        };
    }
}
