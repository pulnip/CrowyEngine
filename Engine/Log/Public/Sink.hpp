#pragma once

#include "LogDefinitions.hpp"
#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    struct LogMessage;

    class Sink{
    public:
        CROWY_DECLARE_INTERFACE(Sink)

        virtual void Write(const LogMessage&) = 0;
    };

    void AddSink(RAII<Sink>);
}
