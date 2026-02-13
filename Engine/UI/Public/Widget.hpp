#pragma once

#ifndef CROWY_UI_CONTEXT
    #define CROWY_UI_CONTEXT UIContext
#endif

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "math.hpp"

namespace Crowy
{
    struct CROWY_UI_CONTEXT;

    struct IntField{
        std::string label;
        std::function<void(CROWY_UI_CONTEXT&, int)> onChanged = [](CROWY_UI_CONTEXT&, int){};
        int v = 0;

        void submit(CROWY_UI_CONTEXT&);
    };

    struct FloatField{
        std::string label;
        std::function<void(CROWY_UI_CONTEXT&, float)> onChanged = [](CROWY_UI_CONTEXT&, float){};
        float v = 0;

        void submit(CROWY_UI_CONTEXT&);
    };

    struct Float2Field{
        std::string label;
        std::function<void(CROWY_UI_CONTEXT&, Vec2)> onChanged = [](CROWY_UI_CONTEXT&, Vec2){};
        Vec2 v{0, 0};

        void submit(CROWY_UI_CONTEXT&);
    };

    struct Float3Field{
        std::string label;
        std::function<void(CROWY_UI_CONTEXT&, Vec3)> onChanged = [](CROWY_UI_CONTEXT&, Vec3){};
        Vec3 v{0, 0, 0};

        void submit(CROWY_UI_CONTEXT&);
    };

    struct Float4Field{
        std::string label;
        std::function<void(CROWY_UI_CONTEXT&, Vec4)> onChanged = [](CROWY_UI_CONTEXT&, Vec4){};
        Vec4 v{0, 0, 0, 0};

        void submit(CROWY_UI_CONTEXT&);
    };

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

    struct SearchBar{
        std::string label;
        std::function<void(CROWY_UI_CONTEXT&, std::string_view)> onChanged = [](CROWY_UI_CONTEXT&, std::string_view){};
        std::string str;

        void submit(CROWY_UI_CONTEXT&);
    };

    struct Box{
    private:
        class IWidget{
        public:
            virtual ~IWidget() = default;

            virtual void submit(CROWY_UI_CONTEXT& ctx) = 0;
            virtual std::unique_ptr<IWidget> clone() const = 0;
        };

        template<typename T>
        class WidgetWrapper: public IWidget{
        private:
            T widget;

        public:
            WidgetWrapper(T widget)
                : widget(std::move(widget)){}
            ~WidgetWrapper() = default;

            void submit(CROWY_UI_CONTEXT& ctx) override{
                widget.submit(ctx);
            }
            std::unique_ptr<IWidget> clone() const override{
                return std::make_unique<WidgetWrapper>(widget);
            }
        };

        std::unique_ptr<IWidget> ptr;

    public:
        template<typename T>
        Box(T&& widget)
            : ptr(std::make_unique<WidgetWrapper<std::decay_t<T>>>(std::forward<T>(widget))){}

        ~Box() = default;
        Box(Box&&) = default;
        Box& operator=(Box&&) = default;
        Box(const Box& other)
            : ptr(other.ptr ? other.ptr->clone() : nullptr){}
        Box& operator=(const Box& other){
            ptr = other.ptr ? other.ptr->clone() : nullptr;
            return *this;
        }

        inline void submit(CROWY_UI_CONTEXT& ctx){
            ptr->submit(ctx);
        }
    };

    using Widget = std::variant<
        TextButton, Checkbox, Slider, Text, SearchBar,
        Box
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