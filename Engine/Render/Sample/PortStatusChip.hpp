#pragma once

#include "CommandPort.hpp"

namespace Crowy
{
    // hidden until the port first connects or fails to bind; the sweep steps
    // with the drain count, never wall time, so a capture repeats its pixels
    void drawPortStatusChip(const CommandPortStatus& status);
}
