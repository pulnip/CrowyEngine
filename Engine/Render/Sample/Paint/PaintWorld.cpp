#include "PaintWorld.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Crowy
{
    namespace
    {
        // the trace a shot takes: far past anything on the stage
        constexpr f64 TraceDistance = 10000.0;
    }

    PaintWorld::PaintWorld()
        : meshes(std::make_unique<PaintStageMeshes>(makePaintStageMeshes())) {
        for(auto& object: makePaintStageObjects()) {
            const auto& mesh = meshes->Get(object.mesh);
            surfaces.emplace_back(std::move(object), mesh);
        }
        refreshActive();
    }

    PaintWorld::~PaintWorld() = default;

    std::optional<PaintHit> PaintWorld::Fire(const PaintShot& shot) {
        const auto hit = scene.Raycast(
            shot.origin,
            shot.velocity,
            TraceDistance,
            shot.onlySurface
        );
        if(!hit)
            return std::nullopt;

        auto splat = shot.brush.BuildSplat(
            *hit,
            shot.velocity,
            shot.paintId,
            shot.volume,
            shot.heightAdd,
            shot.seed
        );
        if(shot.splash) {
            const auto speed = size(shot.velocity);
            splat.incidentDir =
                speed > 1e-4 ? shot.velocity / speed : -hit->impactNormal;
            splat.incidentSpeed = static_cast<u16>(
                std::clamp(std::floor(speed + 0.5), 0.0, 65535.0)
            );
            splat.ballRadius = static_cast<u8>(
                std::clamp(std::floor(shot.ballRadius + 0.5f), 0.0f, 255.0f)
            );
        }
        // FPaintDeposit::MarkTransience: kept only on a direction the hit
        // surface keeps
        const auto& surface = surfaces[static_cast<usize>(hit->surface)];
        splat.transient = !surface.IsWorldNormalPersistent(hit->impactNormal);
        SubmitSplat(splat);

        return hit;
    }

    void PaintWorld::SubmitSplat(const PaintSplat& splat) {
        log.push_back(splat);
        ApplySplat(splat);
    }

    void PaintWorld::ApplySplat(const PaintSplat& splat) {
        if(splat.transient) {
            transients.push_back(
                PaintTransientMark{
                    .location = splat.location,
                    .normal = splat.normal,
                    .radius = splat.radius
                }
            );
            return;
        }
        stampSurfaces(splat);
    }

    void PaintWorld::stampSurfaces(const PaintSplat& splat) {
        // collision, not a registry, decides which surfaces a stamp reaches
        std::vector<usize> reached;
        PaintSceneQuery::OverlapSphere(
            splat.location,
            splat.WorldExtent(),
            surfaces,
            active,
            reached
        );
        for(const auto index: reached) {
            auto& surface = surfaces[index];
            const auto stamp = surface.ComputeLocalStamp(splat);
            // marked first: the score exists even where there is no picture;
            // a draw-only splat is a droplet's mark, its score already claimed
            if(!splat.drawOnly)
                surface.MarkScore(splat, stamp);
            if(splat.scoreOnly || surface.Layout().IsEmpty())
                continue;

            PaintStampDraw draw{
                .surface = index,
                .splat = splat,
                .stamp = stamp,
                .shapeStage = shapeStage
            };
            surface.BuildStampRects(draw.stamp, draw.rects);
            if(!draw.rects.empty())
                draws.push_back(std::move(draw));
        }
    }

    void PaintWorld::SetStage(PaintStageKind kind) {
        stage = kind;
        refreshActive();
    }

    void PaintWorld::SetDirections(
        usize surface,
        u8 mask,
        bool floorFollowsWorldUp
    ) {
        surfaces[surface].SetDirections(mask, floorFollowsWorldUp);
    }

    void PaintWorld::Reset() {
        for(auto& surface: surfaces)
            surface.ClearScore();
        log.clear();
        draws.clear();
        transients.clear();
    }

    PaintCoverage PaintWorld::Coverage() const {
        PaintCoverage coverage;
        for(usize i = 0; i < surfaces.size(); ++i) {
            if(active[i] != 0)
                coverage.Add(surfaces[i].Cells().Coverage());
        }

        return coverage;
    }

    std::vector<PaintStampDraw> PaintWorld::TakeDraws() {
        return std::exchange(draws, {});
    }

    void PaintWorld::refreshActive() {
        active.assign(surfaces.size(), 0);
        for(usize i = 0; i < surfaces.size(); ++i)
            active[i] = surfaces[i].Object().stage == stage ? 1 : 0;
        scene.Rebuild(surfaces, active);
    }
}
