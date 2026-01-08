#pragma once

#include <cstdint>
#include <memory>
#include "RHIFWD.hpp"

namespace Crowy
{
    class InputProvider;
    class MainLoop;

    struct WindowConfig{
        const char* title = "Crowy";
        int width = 800;
        int height = 600;
        bool fullscreen = false;
        bool resizable = true;
        bool borderless = false;
        bool always_on_top = false;
    };

    class OS{
    private:
        class Impl;
        std::unique_ptr<Impl> impl;

        // singleton object
        static OS* instance;

    public:
        OS(const WindowConfig&);
        virtual ~OS();

        virtual void run();
        virtual void processEvents();

        inline static OS* singleton(){ return instance; }

        int getExitCode() const;
        InputProvider* getInputProvider();
        RHIDevice* getDevice();
        RHICommandList* getCommandList();

        void setMainLoop(MainLoop*);
    };
}