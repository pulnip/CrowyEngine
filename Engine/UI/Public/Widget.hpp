#pragma once

#include <functional>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>
#include "Primitives.hpp"

namespace Crowy
{
    // Defined at Lower layer
    struct UIContext;

    struct IntField{
        Str label;
        std::function<void(UIContext&, int)> onChanged = [](UIContext&, int){};
        int v = 0;
        std::optional<std::function<int()>> get = std::nullopt;

        void submit(UIContext&);
    };

    struct FloatField{
        Str label;
        std::function<void(UIContext&, float)> onChanged = [](UIContext&, float){};
        float v = 0;
        std::optional<std::function<float()>> get = std::nullopt;

        void submit(UIContext&);
    };

    struct Float2Field{
        Str label;
        std::function<void(UIContext&, Vec2)> onChanged = [](UIContext&, Vec2){};
        Vec2 v{0, 0};
        std::optional<std::function<Vec2()>> get = std::nullopt;

        void submit(UIContext&);
    };

    struct Float3Field{
        Str label;
        std::function<void(UIContext&, Vec3)> onChanged = [](UIContext&, Vec3){};
        Vec3 v{0, 0, 0};
        std::optional<std::function<Vec3()>> get = std::nullopt;

        void submit(UIContext&);
    };

    struct Float4Field{
        Str label;
        std::function<void(UIContext&, Vec4)> onChanged = [](UIContext&, Vec4){};
        Vec4 v{0, 0, 0, 0};
        std::optional<std::function<Vec4()>> get = std::nullopt;

        void submit(UIContext&);
    };

    struct Checkbox{
        Str label;
        std::function<void(UIContext&, bool)> onChanged = [](UIContext&, bool){};
        bool v;

        void submit(UIContext&);
    };

    struct TextButton{
        Str label;
        std::function<void(UIContext&)> onPressed = [](UIContext&){};

        void submit(UIContext&);
    };

    struct Slider{
        Str label;
        std::function<void(UIContext&, float)> onChanged = [](UIContext&, float){};
        float v = 0.0f;
        float v_min = 0.0f, v_max = 1.0f;

        void submit(UIContext&);
    };

    struct DragFloat{
        Str label;
        std::function<void(UIContext&, float)> onChanged = [](UIContext&, float){};
        float v = 0.0f;
        std::optional<std::function<float()>> get = std::nullopt;
        float v_speed = 0.01f;

        void submit(UIContext&);
    };

    // onChanged carries the index into entries, never the underlying value:
    // an enumerator's value is neither its position nor contiguous, so the
    // mapping back belongs to whoever built the entries.
    struct Dropdown{
        Str label;
        std::function<void(UIContext&, usize)> onChanged = [](UIContext&, usize){};
        std::vector<Str> entries;
        // no entry matches the current value; nothing is selected
        std::optional<usize> current = std::nullopt;

        void submit(UIContext&);
    };

    struct Text{
        Str data;

        void submit(UIContext&);
    };

    struct SearchBar{
        Str label;
        std::function<void(UIContext&, StrView)> onChanged = [](UIContext&, StrView){};
        Str str;

        void submit(UIContext&);
    };

    struct Box{
    private:
        class IWidget{
        public:
            virtual ~IWidget() = default;

            virtual void submit(UIContext& ctx) = 0;
            virtual RAII<IWidget> clone() const = 0;
        };

        template<typename T>
        class WidgetWrapper: public IWidget{
        private:
            T widget;

        public:
            WidgetWrapper(T widget)
                : widget(std::move(widget)){}
            ~WidgetWrapper() = default;

            void submit(UIContext& ctx) override{
                widget.submit(ctx);
            }
            RAII<IWidget> clone() const override{
                return std::make_unique<WidgetWrapper>(widget);
            }
        };

        RAII<IWidget> ptr;

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

        void submit(UIContext& ctx){
            ptr->submit(ctx);
        }
    };

    struct Collapsing;

    using Widget = std::variant<
        IntField, FloatField, Float2Field, Float3Field, Float4Field,
        Checkbox, TextButton, Slider, DragFloat, Dropdown, Text, SearchBar,
        Collapsing, Box
    >;

    struct Collapsing{
        Str label;
        std::vector<Widget> children;
        bool defaultOpen = false;
        const void* scopeId = nullptr;

        void submit(UIContext&);
    };

    Widget Row(
        std::vector<Widget>&& children,
        double spacing = 0.0
    );

    Widget Column(
        std::vector<Widget>&& children,
        double spacing = 0.0
    );
}
