#pragma once

#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "MintFrame.hpp"
#include "PaintBrushProfile.hpp"
#include "PaintDropletFlight.hpp"
#include "PaintSceneQuery.hpp"
#include "PaintShared.h"
#include "PaintSplash.hpp"
#include "PaintStage.hpp"
#include "PaintSurface.hpp"
#include "PaintTypes.hpp"
#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    // one shot: where the ball comes from and how fast, and what it carries
    struct PaintShot {
        DVec3 origin;
        // cm/s; its direction is the trace's
        DVec3 velocity;
        u8 paintId = 0;
        i32 seed = 0;
        f32 volume = 1.0f;
        f32 heightAdd = 0.35f;
        PaintBrushProfile brush;
        // cm; what the splash scales its droplets by
        f32 ballRadius = 6.0f;
        bool splash = false;
        // keeps the trace and the droplets to one surface; -1 for any
        i64 onlySurface = -1;
    };

    // one splat on one surface, as the brush is to draw it
    struct PaintStampDraw {
        usize surface = 0;
        PaintSplat splat;
        PaintLocalStamp stamp;
        std::vector<IntRect> rects;
        // the shape slider's stop; PAINT_SHAPE_STAGE_FULL is StampCustom
        f32 shapeStage = PAINT_SHAPE_STAGE_FULL;
    };

    // a contact on a direction its surface does not keep: an effect only
    struct PaintTransientMark {
        DVec3 location;
        DVec3 normal;
        f32 radius = 0.0f;
    };

    // a splash's footprint for the overlays: a phantom landing the score
    // claimed, or a droplet's mark the picture drew
    struct PaintSplashMark {
        DVec3 point;
        DVec3 normal{0.0, 0.0, 1.0};
        f32 radius = 0.0f;
    };

    // UPaintSubsystem, FPaintDeposit and the splat log: every surface, the
    // trace a shot takes, and the one splat that both the brush and the
    // score are fed from
    class PaintWorld {
    private:
        RAII<PaintStageMeshes> meshes;
        std::vector<PaintSurface> surfaces;
        // 1 where the surface is on the shown stage
        std::vector<u8> active;
        PaintStageKind stage = PaintStageKind::Block;
        PaintSceneQuery scene;
        // every accepted splat, in the authority's order
        std::vector<PaintSplat> log;
        // the brush's work since the last take
        std::vector<PaintStampDraw> draws;
        std::vector<PaintTransientMark> transients;
        f32 shapeStage = PAINT_SHAPE_STAGE_FULL;
        PaintSplashProfile splash = PaintSplashProfile::Paintball();
        std::vector<PaintSplashFlight> flights;
        std::vector<PaintSplashMark> phantoms;
        std::vector<PaintSplashMark> marks;

    public:
        PaintWorld();
        ~PaintWorld();
        CROWY_DECLARE_PINNED(PaintWorld)

        // traces the shot and, on a surface that receives paint, builds
        // the splat, marks it transient or not and submits it; a splashing
        // shot also throws this machine's droplets
        std::optional<PaintHit> Fire(const PaintShot& shot);
        // the droplets in the air, PaintFlightSubstep at a time
        void Step(i32 substeps);
        // the authority's acceptance: logged, then applied
        void SubmitSplat(const PaintSplat& splat);
        // every machine's: a transient one is an effect, a kept one stamps
        // every surface it reaches
        void ApplySplat(const PaintSplat& splat);

        void SetStage(PaintStageKind kind);
        void SetDirections(usize surface, u8 mask, bool floorFollowsWorldUp);
        // the shape slider's stop for the draws made from now on
        void SetShapeStage(f32 stage) noexcept { shapeStage = stage; }
        // no splat, no draw, no mark; the buffers clear on the GPU side
        void Reset();

        std::vector<PaintStampDraw> TakeDraws();
        // UPaintSubsystem::GetWorldCoverage: every shown surface's grid
        PaintCoverage Coverage() const;

        const PaintStageMeshes& Meshes() const noexcept { return *meshes; }
        std::span<const PaintSurface> Surfaces() const noexcept {
            return surfaces;
        }
        std::span<const u8> Active() const noexcept { return active; }
        PaintStageKind Stage() const noexcept { return stage; }
        std::span<const PaintSplat> Log() const noexcept { return log; }
        std::span<const PaintTransientMark> Transients() const noexcept {
            return transients;
        }
        const PaintSplashProfile& Splash() const noexcept { return splash; }
        // the splats already logged keep pointing at it
        void SetSplash(const PaintSplashProfile& profile) { splash = profile; }
        std::span<const PaintSplashFlight> Flights() const noexcept {
            return flights;
        }
        std::span<const PaintSplashMark> Phantoms() const noexcept {
            return phantoms;
        }
        std::span<const PaintSplashMark> Marks() const noexcept {
            return marks;
        }
        const PaintSceneQuery& Scene() const noexcept { return scene; }

    private:
        void stampSurfaces(const PaintSplat& splat);
        // UPaintSubsystem::ApplyPhantomLandings: every machine derives the
        // same score-only landings from the splat alone
        void applyPhantomLandings(const PaintSplat& splat);
        void refreshActive();
    };
}
