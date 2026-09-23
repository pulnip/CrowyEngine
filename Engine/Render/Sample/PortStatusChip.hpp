#pragma once

#include "CommandPort.hpp"

namespace Crowy
{
    // The ambient corner overlay: the engine heard you. Draws nothing until
    // the port has ever accepted a connection, so an env-driven smoke capture
    // is untouched; a port that failed to bind shows regardless. The sweep
    // steps with the drain count, never wall time, so a scripted capture
    // sequence lands on the same pixels every run.
    void drawPortStatusChip(const CommandPortStatus& status);
}
