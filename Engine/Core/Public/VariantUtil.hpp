#pragma once

namespace Crowy
{
    template<class... Ts>
    struct overload: Ts...{
        using Ts::operator()...;
    };
}
