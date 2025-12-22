#pragma once

#include "Sink.hpp"

namespace Crowy
{
    class ConsoleSink: public Sink{
    public:
        void write(const LogMessage&) override;
    };
}