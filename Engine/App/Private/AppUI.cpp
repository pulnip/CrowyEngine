#include <string>
#include <string_view>
#include <utility>
#include "AppUI.hpp"
#include "Context.hpp"
#include "RenderDefinitions.hpp"
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
                for(auto [name, field]: cbuf->fieldViews()){
                    switch(field.type){
                    case CBufferFieldType::Int32:
                        children.push_back(IntField{
                            .label = std::string(name),
                            .onChanged = [field](UIContext&, int v) mutable{
                                field = v;
                            },
                            .v = field
                        });
                        break;
                    case CBufferFieldType::Float:
                        children.push_back(FloatField{
                            .label = std::string(name),
                            .onChanged = [field](UIContext&, float v) mutable{
                                field = v;
                            },
                            .v = field
                        });
                        break;
                    case CBufferFieldType::Float2:
                        children.push_back(Float2Field{
                            .label = std::string(name),
                            .onChanged = [field](UIContext&, Vec2 v) mutable{
                                field = v;
                            },
                            .v = field
                        });
                        break;
                    case CBufferFieldType::Float3:
                        children.push_back(Float3Field{
                            .label = std::string(name),
                            .onChanged = [field](UIContext&, Vec3 v) mutable{
                                field = v;
                            },
                            .v = field
                        });
                        break;
                    case CBufferFieldType::Float4:
                        children.push_back(Float4Field{
                            .label = std::string(name),
                            .onChanged = [field](UIContext&, Vec4 v) mutable{
                                field = v;
                            },
                            .v = field
                        });
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