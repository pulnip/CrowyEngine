#pragma once

#include <expected>

#include "DOM.hpp"
#include "InputEvent.hpp"
#include "InputProvider.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    std::expected<InputEvent, Str> parseInputEvent(const DOM::Value& node);

    DOM::Table inputReport(
        u64 frame,
        const InputProvider& input,
        const InputStatus& inputStatus
    );
}
