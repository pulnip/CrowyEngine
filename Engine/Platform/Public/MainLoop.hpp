#pragma once

#include "semantics.hpp"

namespace Crowy
{
    class MainLoop{
    public:
        DECLARE_INTERFACE(MainLoop)

        virtual void initialize(){}
        virtual bool update(float dt){ return true; }
        virtual void finalize(){}
    };
}