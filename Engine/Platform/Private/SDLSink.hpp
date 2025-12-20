#pragma once

#include "Sink.hpp"

namespace Crowy
{
    class SDLSink: public Sink{
    public:
        void write(const LogMessage&) override;
    };
}