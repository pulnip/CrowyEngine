#pragma once

#include <filesystem>
#include "DomTraits.hpp"

namespace Crowy
{
    enum class JsonStyle : u8{
        Compact,
        Pretty
    };

    DOM::Value parseJsonString(StrView);
    DOM::Value parseJsonFile(const std::filesystem::path&);

    Str emitJson(const DOM::Value&, JsonStyle style = JsonStyle::Compact);

    template<typename T>
    T loadJson(StrView str){
        auto tbl = parseJsonString(str);
        auto metadata = DomTraits<DocMetadata>::from(tbl);

        return DomTraits<T>::from(tbl, metadata);
    }

    template<typename T>
    T loadJsonFile(const std::filesystem::path& path){
        auto tbl = parseJsonFile(path);
        auto metadata = DomTraits<DocMetadata>::from(tbl);

        return DomTraits<T>::from(tbl, metadata);
    }
}
