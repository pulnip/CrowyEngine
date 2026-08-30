#pragma once

#include <filesystem>
#include "DomTraits.hpp"

namespace Crowy
{
    DOM::Value parseTomlString(StrView);
    DOM::Value parseTomlFile(const std::filesystem::path&);

    template<typename T>
    T loadToml(StrView str){
        auto tbl = parseTomlString(str);
        auto metadata = DomTraits<DocMetadata>::from(tbl);

        return DomTraits<T>::from(tbl, metadata);
    }

    template<typename T>
    T loadTomlFile(const std::filesystem::path& path){
        auto tbl = parseTomlFile(path);
        auto metadata = DomTraits<DocMetadata>::from(tbl);

        return DomTraits<T>::from(tbl, metadata);
    }
}
