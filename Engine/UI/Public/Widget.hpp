#pragma once

#ifndef CROWY_UI_CONTEXT
    #define CROWY_UI_CONTEXT UIContext
#endif

#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace Crowy
{
    struct CROWY_UI_CONTEXT;

    struct TextButton{
        std::function<void(CROWY_UI_CONTEXT&)> onPressed;
        std::string label;

        void submit(CROWY_UI_CONTEXT&);
    };

    struct Slider{
        std::string label;
        std::function<void(CROWY_UI_CONTEXT&, float)> onChanged;
        float v = 0.0f;
        float v_min = 0.0f, v_max = 0.0f;

        void submit(CROWY_UI_CONTEXT&);
    };

    struct Text{
        std::string data;

        void submit(CROWY_UI_CONTEXT&){}
    };

    template<typename T>
    struct Box{
        std::unique_ptr<T> ptr;

        void submit(CROWY_UI_CONTEXT& ctx){
            ptr->submit(ctx);
        }
    };

    struct Flex;

    using Widget = std::variant<
        TextButton, Slider, Text,
        Box<Flex>
    >;

    Widget Row(
        std::vector<Widget>&& children,
        double spacing = 0.0
    );

    Widget Column(
        std::vector<Widget>&& children,
        double spacing = 0.0
    );

    Widget demoUI();
}