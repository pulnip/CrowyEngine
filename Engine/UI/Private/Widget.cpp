#include <imgui.h>
#include "Widget.hpp"

namespace Crowy
{
    void TextButton::submit(CROWY_UI_CONTEXT& ctx){
        if(ImGui::Button(label.c_str())){
            onPressed(ctx);
        }
    }

    void Slider::submit(CROWY_UI_CONTEXT& ctx){
        if(ImGui::SliderFloat(label.c_str(), &v, v_min, v_max))
            onChanged(ctx, v);
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
            bool first = true;

            ImGui::BeginGroup();
            for(auto& widget: children){
                if(!first && axis == Axis::horizontal){
                    ImGui::SameLine(0, spacing);
                    first = false;
                }

                std::visit([&ctx](auto& widget){
                    widget.submit(ctx);
                }, widget);
            }
            ImGui::EndGroup();
        }
    };

    Widget Row(
        std::vector<Widget>&& children,
        double spacing
    ){
        return Box<Flex>{
            .ptr = std::make_unique<Flex>(Flex{
                .axis = Axis::horizontal,
                .children = std::move(children)
            })
        };
    }

    Widget Column(
        std::vector<Widget>&& children,
        double spacing
    ){
        return Box<Flex>{
            .ptr = std::make_unique<Flex>(Flex{
                .axis = Axis::vertical,
                .children = std::move(children)
            })
        };
    }
}