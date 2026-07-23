#include "TomlLoader.hpp"

namespace Crowy
{
    TomlMetadata TomlTraits<TomlMetadata>::from(
        const DOM::Value& root
    ){
        auto version = root.get<u32>("metadata.version")
            .value_or(0);
        auto type = root.get<Str>("metadata.type")
            .value_or("Unknown");
        auto name = root.get<Str>("metadata.name")
            .value_or("Unnamed");

        return TomlMetadata{
            .version = version,
            .type = type,
            .name = name,
        };
    }
}
