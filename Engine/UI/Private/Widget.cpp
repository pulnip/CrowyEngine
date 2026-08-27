#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include "Widget.hpp"
#include "Primitives.hpp"

namespace{
    enum class Axis{
        horizontal,
        vertical
    };

    struct Flex{
        Axis axis;
        std::vector<Crowy::Widget> children;
        double spacing = 0.0;

        void submit(Crowy::UIContext& ctx){
            ImGui::BeginGroup();
            for(Crowy::usize i = 0; i < children.size(); ++i){
                if(i > 0 && axis == Axis::horizontal)
                    ImGui::SameLine(0, spacing);
                if(i > 0 && axis == Axis::vertical && spacing > 0.0)
                    ImGui::Dummy(ImVec2(0.0f, static_cast<float>(spacing)));

                ImGui::PushID(static_cast<int>(i));
                std::visit([&ctx](auto& widget){
                    widget.submit(ctx);
                }, children[i]);
                ImGui::PopID();
            }
            ImGui::EndGroup();
        }
    };
}

namespace Crowy
{
    void IntField::submit(UIContext& ctx){
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

    void FloatField::submit(UIContext& ctx){
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

    void Float2Field::submit(UIContext& ctx){
        bool readonly = get.has_value();

        if(readonly){
            ImGui::BeginDisabled();
            v = get.value()();
        }

        f32 arr[] = {v.x, v.y};

        if(ImGui::InputFloat2(label.c_str(), arr)){
            v = {arr[0], arr[1]};
            onChanged(ctx, v);
        }

        if(readonly){
            ImGui::EndDisabled();
        }
    }

    void Float3Field::submit(UIContext& ctx){
        bool readonly = get.has_value();

        if(readonly){
            ImGui::BeginDisabled();
            v = get.value()();
        }

        f32 arr[] = {v.x, v.y, v.z};

        if(ImGui::InputFloat3(label.c_str(), arr)){
            v = {arr[0], arr[1], arr[2]};
            onChanged(ctx, v);
        }

        if(readonly){
            ImGui::EndDisabled();
        }
    }

    void Float4Field::submit(UIContext& ctx){
        bool readonly = get.has_value();

        if(readonly){
            ImGui::BeginDisabled();
            v = (*get)();
        }

        f32 arr[] = {v.x, v.y, v.z, v.w};

        if(ImGui::InputFloat4(label.c_str(), arr)){
            v = {arr[0], arr[1], arr[2], arr[3]};
            onChanged(ctx, v);
        }

        if(readonly){
            ImGui::EndDisabled();
        }
    }

    void Checkbox::submit(UIContext& ctx){
        if(ImGui::Checkbox(label.c_str(), &v))
            onChanged(ctx, v);
    }

    void TextButton::submit(UIContext& ctx){
        if(ImGui::Button(label.c_str())){
            onPressed(ctx);
        }
    }

    void Slider::submit(UIContext& ctx){
        if(ImGui::SliderFloat(label.c_str(), &v, v_min, v_max))
            onChanged(ctx, v);
    }

    void DragFloat::submit(UIContext& ctx){
        bool readonly = get.has_value();

        if(readonly){
            ImGui::BeginDisabled();
            v = (*get)();
        }

        if(ImGui::DragFloat(label.c_str(), &v, v_speed)){
            onChanged(ctx, v);
        }

        if(readonly){
            ImGui::EndDisabled();
        }
    }

    void Text::submit(UIContext&){
        ImGui::Text("%s", data.c_str());
    }

    void SearchBar::submit(UIContext& ctx){
        if(ImGui::InputText(label.c_str(), &str))
            onChanged(ctx, str);
    }

    void Collapsing::submit(UIContext& ctx){
        if(scopeId != nullptr)
            ImGui::PushID(scopeId);

        const auto flags = defaultOpen ?
            ImGuiTreeNodeFlags_DefaultOpen :
            ImGuiTreeNodeFlags_None;
        if(ImGui::CollapsingHeader(label.c_str(), flags)){
            for(usize i = 0; i < children.size(); ++i){
                ImGui::PushID(static_cast<int>(i));
                std::visit([&ctx](auto& widget){
                    widget.submit(ctx);
                }, children[i]);
                ImGui::PopID();
            }
        }

        if(scopeId != nullptr)
            ImGui::PopID();
    }

    Widget Row(
        std::vector<Widget>&& children,
        double spacing
    ){
        return Box(Flex{
            .axis = Axis::horizontal,
            .children = std::move(children),
            .spacing = spacing
        });
    }

    Widget Column(
        std::vector<Widget>&& children,
        double spacing
    ){
        return Box(Flex{
            .axis = Axis::vertical,
            .children = std::move(children),
            .spacing = spacing
        });
    }
}
