#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include "Widget.hpp"

namespace Crowy
{
    void IntField::submit(CROWY_UI_CONTEXT& ctx){
        bool readonly = get.has_value();

        if(readonly){
            ImGui::BeginDisabled();
            v = get.value()();
        }

        if(ImGui::InputInt(label.c_str(), &v)){
            onChanged(ctx, v);
        }

        if(readonly){
            ImGui::EndDisabled();
        }
    }

    void FloatField::submit(CROWY_UI_CONTEXT& ctx){
        bool readonly = get.has_value();

        if(readonly){
            ImGui::BeginDisabled();
            v = get.value()();
        }

        if(ImGui::InputFloat(label.c_str(), &v)){
            onChanged(ctx, v);
        }

        if(readonly){
            ImGui::EndDisabled();
        }
    }

    void Float2Field::submit(CROWY_UI_CONTEXT& ctx){
        bool readonly = get.has_value();

        if(readonly){
            ImGui::BeginDisabled();
            v = get.value()();
        }

        float arr[] = {v.x, v.y};

        if(ImGui::InputFloat2(label.c_str(), arr)){
            v = {arr[0], arr[1]};
            onChanged(ctx, v);
        }

        if(readonly){
            ImGui::EndDisabled();
        }
    }

    void Float3Field::submit(CROWY_UI_CONTEXT& ctx){
        bool readonly = get.has_value();

        if(readonly){
            ImGui::BeginDisabled();
            v = get.value()();
        }

        float arr[] = {v.x, v.y, v.z};

        if(ImGui::InputFloat3(label.c_str(), arr)){
            v = {arr[0], arr[1], arr[2]};
            onChanged(ctx, v);
        }

        if(readonly){
            ImGui::EndDisabled();
        }
    }

    void Float4Field::submit(CROWY_UI_CONTEXT& ctx){
        bool readonly = get.has_value();

        if(readonly){
            ImGui::BeginDisabled();
            v = (*get)();
        }

        float arr[] = {v.x, v.y, v.z, v.w};

        if(ImGui::InputFloat2(label.c_str(), arr)){
            v = {arr[0], arr[1], arr[2], arr[3]};
            onChanged(ctx, v);
        }

        if(readonly){
            ImGui::EndDisabled();
        }
    }

    void TextButton::submit(CROWY_UI_CONTEXT& ctx){
        if(ImGui::Button(label.c_str())){
            onPressed(ctx);
        }
    }

    void Checkbox::submit(CROWY_UI_CONTEXT& ctx){
        if(ImGui::Checkbox(label.c_str(), &v))
            onChanged(ctx, v);
    }

    void Slider::submit(CROWY_UI_CONTEXT& ctx){
        if(ImGui::SliderFloat(label.c_str(), &v, v_min, v_max))
            onChanged(ctx, v);
    }

    void Text::submit(CROWY_UI_CONTEXT&){
        ImGui::Text("%s", data.c_str());
    }

    void SearchBar::submit(CROWY_UI_CONTEXT& ctx){
        if(ImGui::InputText(label.c_str(), &str))
            onChanged(ctx, str);
    }

    enum class Axis{
        horizontal,
        vertical
    };

    struct Flex{
        Axis axis;
        std::vector<Widget> children;
        double spacing = 0.0;

        void submit(CROWY_UI_CONTEXT& ctx){
            ImGui::BeginGroup();
            for(size_t i = 0; i < children.size(); ++i){
                if(i > 0 && axis == Axis::horizontal)
                    ImGui::SameLine(0, spacing);

                std::visit([&ctx](auto& widget){
                    widget.submit(ctx);
                }, children[i]);
            }
            ImGui::EndGroup();
        }
    };

    Widget Row(
        std::vector<Widget>&& children,
        double spacing
    ){
        return Box(Flex{
            .axis = Axis::horizontal,
            .children = std::move(children)
        });
    }

    Widget Column(
        std::vector<Widget>&& children,
        double spacing
    ){
        return Box(Flex{
            .axis = Axis::vertical,
            .children = std::move(children)
        });
    }

    Widget demoUI(){
        return Column({
            Row({
                Text{
                    .data = "Hello, "
                },
                Text{
                    .data = "Widget!"
                }
            }),
            TextButton{
                .label = "Test Button",
            },
            Slider{
                .label = "Test Slider",
                .v = 0.5f
            }
        });
    }
}