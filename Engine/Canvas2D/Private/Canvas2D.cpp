#include <utility>
#include "Canvas2D.hpp"

namespace Crowy
{
    // each platform should implement this function
    Canvas2DRAII CreateImGuiCanvas();

    Canvas2DRAII CreateCanvas(RHIDevice& device, CanvasBackend backend){
        using enum CanvasBackend;

        switch(backend){
        case ImGui:    return CreateImGuiCanvas();
        default:
            std::unreachable();
        }
    }
}
