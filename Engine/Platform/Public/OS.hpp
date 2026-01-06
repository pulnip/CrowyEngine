#pragma once

#include <cstdint>
#include <memory>
#include "RHIFWD.hpp"

namespace Crowy
{
    class InputProvider;
    class MainLoop;

    class OS{
    private:
        struct Impl;
        std::unique_ptr<Impl> impl;

        // singleton object
        static OS* instance;

        MainLoop* mainLoop = nullptr;
        bool forceQuit = false;
        int exitCode = 0;

    public:
        OS();
        virtual ~OS();

        virtual void run();
        virtual void processEvents();

        int getExitCode() const{
            return exitCode;
        }

        virtual uint64_t getTicks_us();
        virtual uint64_t getTicks_ms();
        virtual uint64_t getTicks_ns();

        inline static OS* singleton(){ return instance; }

        inline void setMainLoop(MainLoop* mainLoop){
            this->mainLoop = mainLoop;
        }

        RHIDevice* getDevice();
        InputProvider* getInputProvider();
    };
}