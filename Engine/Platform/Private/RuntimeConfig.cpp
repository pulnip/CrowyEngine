#include "RuntimeConfig.hpp"
#include "DOM.hpp"
#include "TomlLoader.hpp"

namespace Crowy
{
    WindowConfig TomlTraits<WindowConfig>::from(
        const DOM::Value& root
    ){
        auto title = root.get<Str>("runtime.window.title")
            .value_or("DefaultWindow");
        auto width = root.get<u32>("runtime.window.width")
            .value_or(800);
        auto height = root.get<u32>("runtime.window.height")
            .value_or(600);

        // Window Options
        auto fullscreen = root.get<bool>("runtime.window.fullscreen")
            .value_or(false);
        auto resizable = root.get<bool>("runtime.window.resizable")
            .value_or(false);
        auto borderless = root.get<bool>("runtime.window.borderless")
            .value_or(false);
        auto always_on_top = root.get<bool>("runtime.window.always_on_top")
            .value_or(false);

        return WindowConfig{
            .title = title,
            .width = width, .height = height,

            .fullscreen = fullscreen,
            .resizable = resizable,
            .borderless = borderless,
            .always_on_top = always_on_top
        };
    }

    RuntimeConfig TomlTraits<RuntimeConfig>::from(
        const DOM::Value& root
    ){
        auto name = root.get<Str>("runtime.app_name")
            .value_or("AnonymousApp");
        auto version = root.get<Str>("runtime.app_version")
            .value_or("v0.0.1");
        auto identifier = root.get<Str>("runtime.app_identifier")
            .value_or("AnonymousIdentifier");

        return RuntimeConfig{
            .name = name,
            .version = version,
            .identifier = identifier,
            .window = TomlTraits<WindowConfig>::from(root)
        };
    }
}
