#pragma once

#include "DOM.hpp"
#include "Primitives.hpp"
#include "TomlLoader.hpp"

namespace Crowy
{
    struct WindowConfig{
        Str title = "DefaultWindow";
        u32 width = 800, height = 600;

        bool fullscreen = false;
        bool resizable = false;
        bool borderless = false;
        bool always_on_top = false;
    };

    template<>
    struct TomlTraits<WindowConfig>{
        static WindowConfig from(const DOM::Value&);
    };

    struct RuntimeConfig{
        Str name = "AnonymousApp";
        Str version = "v0.0.1";
        Str identifier = "AnonymousIdentifier";

        WindowConfig window;
    };

    template<>
    struct TomlTraits<RuntimeConfig>{
        static RuntimeConfig from(const DOM::Value&);
    };
}
