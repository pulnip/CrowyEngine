#include "PaintWorld.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "MintRandom.hpp"

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
            splat.splash = &splash;
            const auto speed = size(shot.velocity);
            splat.incidentDir = speed > DoubleKindaSmallNumber
                                    ? shot.velocity / speed
                                    : -hit->impactNormal;
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

        // the picture's splash starts from the true contact and the full
        // seed; the score's from the splat
        if(shot.splash) {
            auto flight = launchSplash(
                splash,
                PaintSplashInput{
                    .impactPoint = hit->impactPoint,
                    .impactNormal = hit->impactNormal,
                    .incidentVelocity = shot.velocity,
                    .ballRadius = shot.ballRadius,
                    .seed = shot.seed
                },
                shot.brush.ComputeRadius(
                    shot.volume,
                    static_cast<f32>(size(shot.velocity))
                ),
                shot.paintId,
                splat.lockGens,
                shot.onlySurface
            );
            if(!flight.droplets.empty())
                flights.push_back(std::move(flight));
        }

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
        if(splat.splash)
            applyPhantomLandings(splat);
    }

    bool PaintWorld::Launch(const PaintShot& shot, f64 lead) {
        const auto hit = scene.Raycast(
            shot.origin,
            shot.velocity,
            TraceDistance,
            shot.onlySurface
        );
        if(!hit)
            return false;
        balls.push_back(
            PaintBall{
                .shot = shot,
                .contact = hit->impactPoint,
                .substepsLeft = static_cast<i32>(
                    std::llround(std::max(lead, 0.0) / PaintFlightSubstep)
                )
            }
        );

        return true;
    }

    void PaintWorld::Step(i32 substeps) {
        std::vector<PaintDropletLanding> landings;
        for(i32 step = 0; step < substeps; ++step) {
            for(auto& ball: balls)
                --ball.substepsLeft;
            // every ball due, in launch order, so the log is the same however
            // the frames fall; Fire never touches the queue
            for(usize i = 0; i < balls.size();) {
                if(balls[i].substepsLeft > 0) {
                    ++i;
                    continue;
                }
                const auto shot = balls[i].shot;
                balls.erase(balls.begin() + static_cast<std::ptrdiff_t>(i));
                Fire(shot);
            }
            for(auto& flight: flights) {
                landings.clear();
                stepFlight(flight, splash, scene, landings);
                for(const auto& landing: landings) {
                    const auto mark = landingMark(flight, splash, landing);
                    if(!mark)
                        continue;
                    ApplySplat(*mark);
                    marks.push_back(
                        PaintSplashMark{
                            .point = mark->location,
                            .normal = mark->normal,
                            .radius = mark->radius
                        }
                    );
                }
            }
            std::erase_if(flights, [](const PaintSplashFlight& flight) {
                return !flight.IsAlive();
            });
        }
    }

    void PaintWorld::applyPhantomLandings(const PaintSplat& splat) {
        std::vector<PaintPhantomLanding> landings;
        phantomLandings(
            *splat.splash,
            PaintSplashInput{
                .impactPoint = splat.location,
                .impactNormal = splat.normal,
                .incidentVelocity =
                    splat.incidentDir * static_cast<f64>(splat.incidentSpeed),
                .ballRadius = splat.ballRadius > 0
                                  ? static_cast<f32>(splat.ballRadius)
                                  : 6.0f,
                .seed = splat.seed
            },
            PaintGravityZ,
            landings
        );
        for(usize i = 0; i < landings.size(); ++i) {
            const auto radius =
                splat.splash->ComputeMarkRadius(landings[i].speed) *
                splat.splash->phantomCellRadiusScale;
            if(radius <= 0.0f)
                continue;

            // round, on the contact's own plane, and never splashing again
            auto phantom = splat;
            phantom.splash = nullptr;
            phantom.scoreOnly = true;
            phantom.location = landings[i].point;
            phantom.radius = radius;
            phantom.stretch = 1.0f;
            phantom.impactU = 0.0f;
            phantom.seed = static_cast<u16>(
                hashCombineFast(splat.seed, static_cast<u32>(i + 1)) & 0xFFFFu
            );
            stampSurfaces(phantom);
            phantoms.push_back(
                PaintSplashMark{
                    .point = phantom.location,
                    .normal = phantom.normal,
                    .radius = radius
                }
            );
        }
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
        // the queued stamps were cut for the atlas this replaces
        std::erase_if(draws, [surface](const PaintStampDraw& draw) {
            return draw.surface == surface;
        });
    }

    void PaintWorld::Reset() {
        for(auto& surface: surfaces)
            surface.ClearScore();
        log.clear();
        draws.clear();
        transients.clear();
        flights.clear();
        balls.clear();
        phantoms.clear();
        marks.clear();
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
