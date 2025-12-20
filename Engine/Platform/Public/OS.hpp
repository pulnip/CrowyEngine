#pragma once

#include <cstdint>
#include <memory>

namespace Crowy
{
    class MainLoop;

    class OS{
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

        inline static OS* get(){ return os; }

        inline void setMainLoop(MainLoop* mainLoop){
            this->mainLoop = mainLoop;
        }

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;

        // singleton object
        static OS* os;

        MainLoop* mainLoop = nullptr;
        bool forceQuit = false;
        int exitCode = 0;
    };
}