#pragma once

#include "EnumUtil.hpp"
#include "PaintShared.h"
#include "Primitives.hpp"

// What PaintLab shows and how, reflected for its panel and its port
namespace Crowy
{
    // the surface pass's view, PaintShared.h's PAINT_VIEW_* in order
    enum class PaintView : u32 {
        Lit,
        Islands,
        PaintId,
        Height,
        Distance,
        Position,
        EdgeFade,
        Score,
        Divergence,
        SignedDistance,
        Coverage,
        HeightField,
        Normal,
        BaseColor,
        Roughness,
        ShadingModel,
    };
    static_assert(static_cast<u32>(PaintView::Islands) == PAINT_VIEW_ISLANDS);
    static_assert(
        static_cast<u32>(PaintView::EdgeFade) == PAINT_VIEW_EDGE_FADE
    );
    static_assert(
        static_cast<u32>(PaintView::ShadingModel) == PAINT_VIEW_SHADING_MODEL
    );

    CROWY_ENUM_BEGIN(PaintView)
    CROWY_ENUM_VALUE(Lit)
    CROWY_ENUM_VALUE(Islands)
    CROWY_ENUM_VALUE(PaintId)
    CROWY_ENUM_VALUE(Height)
    CROWY_ENUM_VALUE(Distance)
    CROWY_ENUM_VALUE(Position)
    CROWY_ENUM_VALUE(EdgeFade)
    CROWY_ENUM_VALUE(Score)
    CROWY_ENUM_VALUE(Divergence)
    CROWY_ENUM_VALUE(SignedDistance)
    CROWY_ENUM_VALUE(Coverage)
    CROWY_ENUM_VALUE(HeightField)
    CROWY_ENUM_VALUE(Normal)
    CROWY_ENUM_VALUE(BaseColor)
    CROWY_ENUM_VALUE(Roughness)
    CROWY_ENUM_VALUE(ShadingModel)
    CROWY_ENUM_END()

    // the atlas panel's channel, PAINT_PANEL_* in order
    enum class PaintPanelChannel : u32 {
        Islands,
        PaintId,
        Height,
        Distance,
        Position,
        EdgeFade,
    };
    static_assert(
        static_cast<u32>(PaintPanelChannel::EdgeFade) == PAINT_PANEL_EDGE_FADE
    );

    CROWY_ENUM_BEGIN(PaintPanelChannel)
    CROWY_ENUM_VALUE(Islands)
    CROWY_ENUM_VALUE(PaintId)
    CROWY_ENUM_VALUE(Height)
    CROWY_ENUM_VALUE(Distance)
    CROWY_ENUM_VALUE(Position)
    CROWY_ENUM_VALUE(EdgeFade)
    CROWY_ENUM_END()

    enum class PaintPanel : u32 {
        None,
        Atlas,
    };

    CROWY_ENUM_BEGIN(PaintPanel)
    CROWY_ENUM_VALUE(None)
    CROWY_ENUM_VALUE(Atlas)
    CROWY_ENUM_END()

    enum class PaintStageChoice : u32 {
        Block,
        Grid,
    };

    CROWY_ENUM_BEGIN(PaintStageChoice)
    CROWY_ENUM_VALUE(Block)
    CROWY_ENUM_VALUE(Grid)
    CROWY_ENUM_END()

    // exposed as `lab`
    struct PaintLabSettings {
        PaintView view = PaintView::Lit;
        // right of the split
        PaintView compareView = PaintView::Lit;
        // the share of the width `view` takes; 1 shows it alone
        f32 split = 1.0f;
        PaintStageChoice stage = PaintStageChoice::Block;
        PaintPanel panel = PaintPanel::None;
        PaintPanelChannel panelChannel = PaintPanelChannel::Islands;
        // the surface the panel shows, an index into the stage's objects
        i32 selected = 1;
        // the island rectangles and names over the panel
        bool labels = true;
    };

    // a paintable's direction flags, exposed as `stage.<name>`
    struct PaintObjectFlags {
        bool front = false;
        bool back = false;
        bool right = false;
        bool left = false;
        bool up = true;
        bool down = false;
        bool floorFollowsWorldUp = true;
    };
}
