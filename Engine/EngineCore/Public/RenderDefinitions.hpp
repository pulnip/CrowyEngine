#pragma once

#include <string>

namespace Crowy
{
    using RenderType = std::string;
    using RenderTypeHash = std::invoke_result_t<std::hash<RenderType>, RenderType>;
}