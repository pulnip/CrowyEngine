#pragma once

#include "LogDefinitions.hpp"

namespace Crowy
{
    class Sink{
    public:
        virtual ~Sink() = default;
        virtual void write(const LogMessage&) = 0;
    };
}