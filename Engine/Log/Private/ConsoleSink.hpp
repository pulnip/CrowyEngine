#pragma once

#include "Sink.hpp"

namespace Crowy
{
    class ConsoleSink: public Sink{
    public:
        void Write(const LogMessage&) override;
    };
}
