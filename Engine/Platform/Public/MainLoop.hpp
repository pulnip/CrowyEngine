#pragma once

namespace Crowy
{
    class MainLoop{
    public:
        virtual ~MainLoop() = default;

        virtual void initialize(){}
        virtual bool update(float dt){ return true; }
        virtual void finalize(){}
    };
}