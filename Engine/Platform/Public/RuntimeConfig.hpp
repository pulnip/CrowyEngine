#pragma once

#include "DOM.hpp"
#include "DomTraits.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    struct WindowConfig{
        Str title = "DefaultWindow";
        u32 width = 800, height = 600;
        RHIPixelFormat format = RHIPixelFormat::RGBA8_UNORM_SRGB;

        bool fullscreen = false;
        bool resizable = false;
        bool borderless = false;
        bool always_on_top = false;

        // a timed run wants Present(0, 0): with vsync on, every frame time
        // is really the refresh rate talking
        bool vsync = true;
    };

    template<>
    struct DomTraits<WindowConfig>{
        static WindowConfig from(const DOM::Value&);
    };

    // Set by the sample itself, so its source says what the run is for.
    // Only has teeth in a CROWY_BENCHMARK build.
    struct BenchmarkConfig{
        bool enabled = false;
        // dropped, not measured: pipeline creation, shader caches, first-touch
        // page faults and the GPU settling on its clocks all land in here.
        // long enough to cover two seconds of real time
        u32 warmupFrames = 400;
        // measured, then the loop quits on its own
        u32 measureFrames = 1000;

        // both optional; empty means don't write that one
        Str reportPath;  // percentile summary, for reading
        Str framePath;   // one row per frame, for regressing against draw counts
    };

    struct RuntimeConfig{
        Str name = "AnonymousApp";
        Str version = "v0.0.1";
        Str identifier = "AnonymousIdentifier";

        WindowConfig window;
        BenchmarkConfig benchmark;
    };

    template<>
    struct DomTraits<RuntimeConfig>{
        static RuntimeConfig from(const DOM::Value&);
    };
}
