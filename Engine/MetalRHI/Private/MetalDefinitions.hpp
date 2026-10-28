#pragma once

#include <Foundation/NSSharedPtr.hpp>

namespace Crowy
{
    template<typename T>
    using RefPtr = NS::SharedPtr<T>;
}
