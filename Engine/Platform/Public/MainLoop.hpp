#pragma once

#include "semantics.hpp"

namespace Crowy
{
    class MainLoop{
    public:
        DECLARE_INTERFACE(MainLoop)

        virtual void initialize(){}
        virtual bool update(float deltaTime, float totalTime) = 0;
        virtual void finalize(){}
    };
}