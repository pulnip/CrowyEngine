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
        std::string label;
        std::function<void(CROWY_UI_CONTEXT&)> onPressed = [](CROWY_UI_CONTEXT&){};

        void submit(CROWY_UI_CONTEXT&);
    };

    struct Checkbox{
        std::string label;
        std::function<void(CROWY_UI_CONTEXT&, bool)> onChanged = [](CROWY_UI_CONTEXT&, bool){};
        bool v;

        void submit(CROWY_UI_CONTEXT&);
    };

    struct Slider{
        std::string label;
        std::function<void(CROWY_UI_CONTEXT&, float)> onChanged = [](CROWY_UI_CONTEXT&, float){};
        float v = 0.0f;
        float v_min = 0.0f, v_max = 1.0f;

        void submit(CROWY_UI_CONTEXT&);
    };

    struct Text{
        std::string data;

        void submit(CROWY_UI_CONTEXT&);
    };

    template<typename T>
    struct Box{
        std::unique_ptr<T> ptr;

        Box() = default;
        ~Box() = default;
        Box(Box&&) = default;
        Box& operator=(Box&&) = default;
        Box(const Box& other)
            : ptr(other.ptr ? std::make_unique<T>(*other.ptr) : nullptr){}
        Box& operator=(const Box& other){
            ptr = other.ptr ? std::make_unique<T>(*other.ptr) : nullptr;
            return *this;
        }

        Box(T&& widget)
            :ptr(std::make_unique<T>(std::move(widget))){}

        void submit(CROWY_UI_CONTEXT& ctx){
            ptr->submit(ctx);
        }
    };

    struct Flex;

    using Widget = std::variant<
        TextButton, Checkbox, Slider, Text,
        Box<Flex>
    >;

        enum class Axis{
        horizontal,
        vertical
    };

    struct Flex{
        Axis axis;
        std::vector<Widget> children;
        double spacing = 0.0;

        void submit(CROWY_UI_CONTEXT& ctx);
    };

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