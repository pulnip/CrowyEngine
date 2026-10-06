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
        Profile,
    };
    static_assert(
        static_cast<u32>(PaintPanelChannel::EdgeFade) == PAINT_PANEL_EDGE_FADE
    );
    static_assert(
        static_cast<u32>(PaintPanelChannel::Profile) == PAINT_PANEL_PROFILE
    );

    CROWY_ENUM_BEGIN(PaintPanelChannel)
    CROWY_ENUM_VALUE(Islands)
    CROWY_ENUM_VALUE(PaintId)
    CROWY_ENUM_VALUE(Height)
    CROWY_ENUM_VALUE(Distance)
    CROWY_ENUM_VALUE(Position)
    CROWY_ENUM_VALUE(EdgeFade)
    CROWY_ENUM_VALUE(Profile)
    CROWY_ENUM_END()

    enum class PaintPanel : u32 {
        None,
        Atlas,
        ShapeLab,
    };

    CROWY_ENUM_BEGIN(PaintPanel)
    CROWY_ENUM_VALUE(None)
    CROWY_ENUM_VALUE(Atlas)
    CROWY_ENUM_VALUE(ShapeLab)
    CROWY_ENUM_END()

    enum class PaintStageChoice : u32 {
        Block,
        Grid,
    };

    CROWY_ENUM_BEGIN(PaintStageChoice)
    CROWY_ENUM_VALUE(Block)
    CROWY_ENUM_VALUE(Grid)
    CROWY_ENUM_END()

    // MintChoco's brush profiles a shot can carry
    enum class PaintBrushChoice : u32 {
        Default,
        Paintball,
        MopT,
        Smooth,
    };

    CROWY_ENUM_BEGIN(PaintBrushChoice)
    CROWY_ENUM_VALUE(Default)
    CROWY_ENUM_VALUE(Paintball)
    CROWY_ENUM_VALUE(MopT)
    CROWY_ENUM_VALUE(Smooth)
    CROWY_ENUM_END()

    // the next shot's contact, exposed as `splat`
    struct PaintShotSettings {
        PaintBrushChoice brush = PaintBrushChoice::Default;
        i32 team = 0;
        // 0..65535; advanced after every interactive shot while autoSeed
        i32 seed = 1;
        bool autoSeed = true;
        // the deposit's volume: radius grows with its square root
        f32 volume = 1.0f;
        f32 heightAdd = 0.35f;
        // cm/s
        f32 speed = 3000.0f;
        // degrees from the normal the shape lab and scripted shots arrive at
        f32 theta = 0.0f;
        // the shape slider: 0 a circle, 8 MintChoco's stamp
        f32 shapeStage = PAINT_SHAPE_STAGE_FULL;
        bool splash = false;
    };

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
