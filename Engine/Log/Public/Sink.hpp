#pragma once

#include "semantics.hpp"
#include "LogDefinitions.hpp"

namespace Crowy
{
    class Sink{
    public:
        CROWY_DECLARE_INTERFACE(Sink)

        virtual void write(const LogMessage&) = 0;
    };
}