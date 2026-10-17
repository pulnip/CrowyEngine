#pragma once

#include <expected>
#include <stdexcept>
#include <utility>

#include "Primitives.hpp"

namespace Crowy
{
    // For a failure that is a bug where it happens, such as geometry the code
    // itself builds: the caller stops with the reason.
    template<typename T>
    T valueOrThrow(std::expected<T, Str> result) {
        if(!result)
            throw std::runtime_error(std::move(result).error());

        return std::move(result).value();
    }
}
