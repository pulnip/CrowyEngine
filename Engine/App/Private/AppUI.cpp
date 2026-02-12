#include <string>
#include <string_view>
#include <utility>
#include "AppUI.hpp"
#include "CBuffer.hpp"
#include "Context.hpp"
#include "Renderer.hpp"
#include "Widget.hpp"

namespace Crowy
{
    struct CBufferInspector{
        std::string searchStr;

        void submit(UIContext& ctx){
            std::vector<Widget> children;

            children.push_back(SearchBar{
                .label = "CBuffer",
                .onChanged = [this](UIContext& ctx, std::string_view str){
                    searchStr = str;
                },
                .str = searchStr
            });
            if(auto cbuf = ctx.renderer.getCBuffer(searchStr)){
                for(auto v: cbuf->fieldViews()){
                    switch(v.field.type){
                    case CBufferFieldType::Int32:
                        break;
                    case CBufferFieldType::Float:
                        children.push_back(Slider{
                            .label = std::string(v.name),
                            .onChanged = [field = v.field](UIContext&, float v) mutable{
                                field = v;
                            },
                            .v = v.field
                        });
                        break;
                    case CBufferFieldType::Float2:
                        break;
                    case CBufferFieldType::Float3:
                        break;
                    case CBufferFieldType::Float4:
                        break;
                    case CBufferFieldType::Float4x4:
                        break;
                    default:
                        std::unreachable();
                        break;
                    }
                }
            }

            auto w = Column(std::move(children));
            std::visit([&ctx](auto& widget){
                widget.submit(ctx);
            }, w);
        }
    };

    Widget cbufferInspector(std::string_view initCBuf){
        return Box(CBufferInspector{
            .searchStr = std::string(initCBuf)
        });
    }
}