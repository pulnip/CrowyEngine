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

    // how the edge between paint and bare is read
    enum class PaintEdgeMode : u32 {
        Nearest,
        Bilinear,
        SignedDistance,
    };

    CROWY_ENUM_BEGIN(PaintEdgeMode)
    CROWY_ENUM_VALUE(Nearest)
    CROWY_ENUM_VALUE(Bilinear)
    CROWY_ENUM_VALUE(SignedDistance)
    CROWY_ENUM_END()

    // how teams lay over each other: by their own coverage, or by what the
    // earlier teams left over
    enum class PaintTeamBlend : u32 {
        Naive,
        ConsumedCoverage,
    };

    CROWY_ENUM_BEGIN(PaintTeamBlend)
    CROWY_ENUM_VALUE(Naive)
    CROWY_ENUM_VALUE(ConsumedCoverage)
    CROWY_ENUM_END()

    // how G is read back, and its slope with it
    enum class PaintHeightFilter : u32 {
        Nearest,
        Bilinear,
        BSpline,
    };

    CROWY_ENUM_BEGIN(PaintHeightFilter)
    CROWY_ENUM_VALUE(Nearest)
    CROWY_ENUM_VALUE(Bilinear)
    CROWY_ENUM_VALUE(BSpline)
    CROWY_ENUM_END()

    // what the relief is built from: the accumulated G, or the painted area
    // alone, as before G existed
    enum class PaintHeightSource : u32 {
        Accumulated,
        PaintedArea,
    };

    CROWY_ENUM_BEGIN(PaintHeightSource)
    CROWY_ENUM_VALUE(Accumulated)
    CROWY_ENUM_VALUE(PaintedArea)
    CROWY_ENUM_END()

    // the surface's look, exposed as `look`; the defaults are MintChoco's
    struct PaintLookSettings {
        PaintEdgeMode edgeMode = PaintEdgeMode::SignedDistance;
        PaintTeamBlend teamBlend = PaintTeamBlend::ConsumedCoverage;
        PaintHeightFilter heightFilter = PaintHeightFilter::BSpline;
        PaintHeightSource heightSource = PaintHeightSource::Accumulated;
        bool edgeFade = true;
        // the slab's lobes
        bool diffuse = true;
        bool specular = true;
        bool haze = true;
        bool fuzz = true;
        bool sss = true;
        bool coat = true;
        bool sky = true;
        // Unreal's Blendable GBuffer as its source says it folds the slab
        bool blendableGBuffer = false;
        // MPC_PaintStyle
        f32 flow = 1.0f;
        f32 normalStrength = 1.0f;
        f32 coatScale = 0.4f;
        f32 fuzzScale = 1.5f;
        f32 roughnessBias = 0.12f;
        f32 coatRoughness = 0.12f;
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
