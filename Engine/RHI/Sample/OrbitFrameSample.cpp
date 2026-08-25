#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <utility>
#include <vector>
#include "AppFramework.hpp"
#include "InputProvider.hpp"
#include "Kepler.hpp"
#include "LinearAlgebra.hpp"
#include "AsteroidBelt.hpp"
#include "OrbitDrawArgs.hpp"
#include "OrbitTrail.hpp"
#include "RHIBuffer.hpp"
#include "RHIPipelineState.hpp"
#include "UIRenderer.hpp"

#include <imgui.h>

namespace Crowy
{
    // Every control the sample has, and the stats it shows. The panel writes
    // straight into this, so a control value lives in exactly one place.
    //
    // Named UIContext because Widget.hpp forward-declares that type and
    // UIRenderer::Prepare takes it, even though this sample drives its panel
    // with ImGui directly and never reads it from a callback.
    struct UIContext{
        // heliocentricity: 0 geocentric, 1 heliocentric. The point of the demo.
        f32 alpha = 0.0f;
        // how much of Earth's orbital angle to cancel as well
        f32 frameLock = 0.0f;

        f32 timeScale = 120.0f;    // sim days per second
        f32 zoomLog = 1.34f;       // half-height in warped units, logged
        f32 orbitTurns = 1.0f;
        f32 thicknessPx = 2.0f;

        std::array<bool, ORBIT_BODY_COUNT> bodyEnabled{};
        bool beltEnabled = true;
        // where every Kepler solve in the frame runs
        bool gpuFill = true;
        f32 beltAlpha = 0.55f;
        bool paused = false;

        // read-only stats
        f32 halfHeight = 1.0f;
        f32 gridStepAU = 1.0f;
        f64 frameTimeMs = 0.0;
        f64 simDay = 0.0;
        u32 filled = 0;
        u64 totalTicks = 0;


        UIContext(){
            bodyEnabled.fill(true);
        }
    };

    // Planet trails drawn as screen-space expanded polylines, straight out of
    // the GPU ring buffer. One draw per body, one instance per segment.
    //
    // The origin is interpolated between Earth and the Sun rather than the
    // camera being moved: the slider is a shader constant, not a view matrix.
    // Push it to 0 and the heliocentric ellipses come apart into Ptolemy's
    // epicycles - Mars doubling back on itself, Venus tracing a rose.
    //
    // Radius is compressed logarithmically after the interpolation, so the
    // warp centre follows whatever the current origin is. 30 AU against
    // 0.39 AU is 77:1 linear and 10:1 through the log.
    //
    // Controls sit in the panel and on the keys at once - the input layer has
    // no scroll wheel, so a zoom slider has to exist, but dragging one is a
    // poor substitute for holding a key while watching the picture move.
    //   Up / Down     zoom
    //   Left / Right  trail length
    //   Space         pause
    //   R             reset to J2000
    class OrbitFrameSample: public App{
        using App::App;

        static constexpr f64 DAY_PER_SAMPLE = 1.0;
        static constexpr f64 START_DAY = 0.0;

        static constexpr Color BACKGROUND{0.02f, 0.02f, 0.05f, 1.0f};

        // Half-heights are in warped units: the whole system reaches
        // log(1 + 30) = 3.43, and Mercury sits at log(1 + 0.39) = 0.33.
        // The ends are picked so each extreme frames something: zoomed all the
        // way out Neptune fills two thirds of the view, all the way in Mercury
        // fills it.
        static constexpr f32 ZOOM_LOG_MIN = -1.2f;   // half-height 0.30
        static constexpr f32 ZOOM_LOG_MAX = 1.7f;    // half-height 5.47

        // The plan says 3.0, but Venus's rose only closes after 13 Venus years
        // (8 Earth years), and that is the shape Step 4 asks to see. Anything
        // that runs past the ring clamps to what is stored, so the ceiling
        // costs nothing.
        static constexpr f32 TURNS_MIN = 0.05f, TURNS_MAX = 15.0f;

        static constexpr f32 TIME_SCALE_MIN = 1.0f, TIME_SCALE_MAX = 2000.0f;
        static constexpr f32 THICKNESS_MIN = 1.0f, THICKNESS_MAX = 6.0f;

        // the camera sits off the ecliptic looking back down +Z; nothing in the
        // scene reaches anywhere near these planes
        static constexpr f32 CAMERA_DISTANCE = 100.0f;
        static constexpr f32 NEAR_Z = 1.0f, FAR_Z = 200.0f;


        // mirrors TrailPush in Engine/Shader/OrbitTrail.slang
        struct TrailPush{
            u64 samples;
            u64 bodies;
            f32 viewportX, viewportY;
            f32 thicknessPx;
            f32 alpha;
            u32 head;
            u32 capacity;
            f32 markerRadiusPx;
            f32 frameLock;
            f32 frameLockBase;
            f32 earthRadPerDay;
            f32 dayPerSample;
            f32 beltRadiusPx;
            u64 belt;
            f32 beltAlpha;
            // dot 0 of this frame's slice; 0 on the GPU path, which owns
            // its buffer outright
            u32 beltBase = 0;
            u64 segCounts;
        };
        static_assert(sizeof(TrailPush) == 88);
        static_assert(offsetof(TrailPush, bodies) == 8);
        static_assert(offsetof(TrailPush, viewportX) == 16);
        static_assert(offsetof(TrailPush, thicknessPx) == 24);
        static_assert(offsetof(TrailPush, alpha) == 28);
        static_assert(offsetof(TrailPush, head) == 32);
        static_assert(offsetof(TrailPush, capacity) == 36);
        static_assert(offsetof(TrailPush, markerRadiusPx) == 40);
        static_assert(offsetof(TrailPush, frameLockBase) == 48);
        static_assert(offsetof(TrailPush, dayPerSample) == 56);
        static_assert(offsetof(TrailPush, belt) == 64);
        static_assert(offsetof(TrailPush, beltAlpha) == 72);
        static_assert(offsetof(TrailPush, beltBase) == 76);
        static_assert(offsetof(TrailPush, segCounts) == 80);

        // mirrors GridPush in Engine/Shader/OrbitGrid.slang
        struct GridPush{
            f32 viewportX, viewportY;
            f32 thinThicknessPx;
            f32 thickThicknessPx;
            f32 colorR, colorG, colorB;
            f32 thinAlpha;
            f32 thickAlpha;
            f32 baseStepAU;
            f32 pxPerWarpUnit;
            f32 spokeRadiusAU;
            f32 minGapPx;
            f32 fadeGapPx;
            f32 referenceAlpha;
            f32 _pad0 = 0.0f;
        };
        static_assert(sizeof(GridPush) == 64);
        static_assert(offsetof(GridPush, colorR) == 16);
        static_assert(offsetof(GridPush, baseStepAU) == 36);
        static_assert(offsetof(GridPush, minGapPx) == 48);
        static_assert(offsetof(GridPush, referenceAlpha) == 56);

        // keep in sync with Engine/Shader/OrbitGrid.slang
        static constexpr u32 GRID_LEVEL_COUNT = 8;
        static constexpr u32 GRID_RINGS_PER_LEVEL = 16;
        static constexpr u32 GRID_RING_SEGMENTS = 192;
        static constexpr u32 GRID_SPOKE_COUNT = 12;
        static constexpr u32 GRID_DRAW_RINGS = 0;
        static constexpr u32 GRID_DRAW_SPOKES = 1;
        static constexpr u32 GRID_DRAW_REFERENCE = 2;

        // The finest ring step is a power of five AU, picked so its rings sit
        // roughly this far apart near the origin. Quantizing to powers of five
        // is what keeps the step a round number a viewer can name - it walks
        // 1 AU, 0.2 AU, 0.04 AU as the zoom goes in.
        static constexpr f32 GRID_TARGET_GAP_PX = 100.0f;
        static constexpr f32 GRID_MIN_GAP_PX = 3.5f;
        static constexpr f32 GRID_FADE_GAP_PX = 11.0f;

        static constexpr f32 MARKER_RADIUS_PX = 4.0f;
        static constexpr f32 BELT_RADIUS_PX = 1.3f;

        struct FrameUniforms{
            Mat4 viewProj;
        };

        // the trail is pulled by index, so the ring's Vec3 packing is the
        // shader's StructuredBuffer<float3> stride
        static_assert(sizeof(Vec3) == 12);

        // warm hues inward, cold outward, and the Sun deliberately not yellow -
        // at alpha 0 it takes over Earth's ring and the two want telling apart
        static constexpr std::array<Vec3, ORBIT_BODY_COUNT> BODY_COLORS{
            Vec3{1.00f, 0.95f, 0.70f},  // Sun
            Vec3{0.78f, 0.72f, 0.66f},  // Mercury
            Vec3{0.95f, 0.72f, 0.40f},  // Venus
            Vec3{0.40f, 0.72f, 1.00f},  // Earth
            Vec3{0.94f, 0.42f, 0.32f},  // Mars
            Vec3{0.90f, 0.76f, 0.55f},  // Jupiter
            Vec3{0.85f, 0.80f, 0.55f},  // Saturn
            Vec3{0.60f, 0.88f, 0.90f},  // Uranus
            Vec3{0.42f, 0.55f, 0.95f}   // Neptune
        };

        RHIDevice* device = nullptr;
        RAII<OrbitTrail> trail;
        AsteroidBelt belt;
        std::vector<Vec3> beltScratch;

        RHIGraphicsPipelineStateRAII trailPSO, gridPSO, markerPSO, beltPSO;
        RHIBufferSlice frameCB;
        RAII<OrbitTrailArgs> trailArgs;
        RHIBufferSlice beltSlice;
        // the compute path cannot write a CPU-write buffer, so the two fills
        // own separate destinations and the draw picks one
        RHIBufferRAII beltGpuBuffer;
        RAII<OrbitKeplerFill> beltFill;
        RHIResourceUsage beltResting = RHIResourceUsage::Undefined;

        RAII<UIRenderer> uiRenderer;
        UIContext context;
        // UIRenderer::Prepare always opens its own window and wants a tree to
        // put in it. This sample draws its panel itself, so it hands over an
        // empty Column and leaves that window blank.
        Widget emptyPanel = Column({});



        f64 pendingSeconds = 0.0;
        f32 viewportWidth = 1.0f, viewportHeight = 1.0f;

        // held between ProcessInput and OnUpdate
        f32 zoomInput = 0.0f, turnsInput = 0.0f;
        bool resetPressed = false;

        static constexpr f32 ZOOM_SPEED = 1.2f;    // log units per second
        static constexpr f32 TURNS_SPEED = 1.5f;   // e-folds per second

        // Every pass in this sample expands screen-space quads out of nothing:
        // no vertex buffer, a strip of four, blended, and unculled because an
        // expanded quad's winding follows whatever direction it was expanded
        // along.
        static RHIGraphicsPipelineStateDesc LineExpansionDesc(
            CStr path,
            CStr vertexEntry,
            CStr fragmentEntry,
            RHIPixelFormat format
        ){
            return RHIGraphicsPipelineStateDesc{
                .preRasterizer = RHILegacyFrontendDesc{
                    .topology = RHIPrimitiveTopology::TriangleStrip,
                    .vertexShader = {
                        .path = path,
                        .entryPoint = vertexEntry
                    }
                },
                .rasterizer = RHIRasterizerState{
                    .cullMode = RHICullMode::None
                },
                .fragmentShader = {
                    .path = path,
                    .entryPoint = fragmentEntry
                },
                .blend = RHIBlendState{
                    .renderTargets = {
                        RHIRenderTargetBlendState{
                            .blendEnable = true,
                            .srcBlend = RHIBlend::SrcAlpha,
                            .dstBlend = RHIBlend::InvSrcAlpha,
                            .blendOp = RHIBlendOp::Add,
                            .srcBlendAlpha = RHIBlend::One,
                            .dstBlendAlpha = RHIBlend::InvSrcAlpha,
                            .blendOpAlpha = RHIBlendOp::Add
                        }
                    }
                },
                .renderTargetFormats = {format},
                .renderTargetCount = 1,
                // SV_StartInstanceLocation needs it
                .profile = "sm_6_8"
            };
        }

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override{
            this->device = &device;

            trail = std::make_unique<OrbitTrail>(
                device,
                DAY_PER_SAMPLE,
                ORBIT_TRAIL_CAPACITY,
                RHIResourceUsage::SampledVertex
            );
            // without this the outer planets would take real minutes to draw
            // themselves in - Neptune's orbit is 60,225 ticks
            trail->SetFillMode(context.gpuFill ?
                OrbitFillMode::Gpu :
                OrbitFillMode::Cpu
            );
            trail->Prefill(START_DAY);

            trailPSO = device.CreatePipelineState(
                LineExpansionDesc(
                    "Engine/Shader/OrbitTrail.slang",
                    "vs_main", "fs_main",
                    swapchain.GetFormat()
                ),
                "OrbitTrailPSO"
            );
            gridPSO = device.CreatePipelineState(
                LineExpansionDesc(
                    "Engine/Shader/OrbitGrid.slang",
                    "vs_main", "fs_main",
                    swapchain.GetFormat()
                ),
                "OrbitGridPSO"
            );
            markerPSO = device.CreatePipelineState(
                LineExpansionDesc(
                    "Engine/Shader/OrbitTrail.slang",
                    "vs_marker", "fs_marker",
                    swapchain.GetFormat()
                ),
                "OrbitMarkerPSO"
            );
            beltPSO = device.CreatePipelineState(
                LineExpansionDesc(
                    "Engine/Shader/OrbitTrail.slang",
                    "vs_belt", "fs_marker",
                    swapchain.GetFormat()
                ),
                "OrbitBeltPSO"
            );


            // rewritten in full every frame, which is the only safe way to use
            // a CPUWrite buffer - see OrbitTrail.hpp for why
            beltScratch.resize(belt.Count());

            beltGpuBuffer = device.CreateBuffer(RHIBufferCreateDesc{
                .size = static_cast<u32>(belt.Count() * sizeof(Vec3)),
                .shaderWrite = true
            }, "OrbitAsteroidBeltGPU");
            beltFill = std::make_unique<OrbitKeplerFill>(
                device, belt.Elements()
            );

            // colour and period only; segCount belongs to the compute pass
            // from here on, and so do the draw arguments beside it
            std::array<OrbitBodyDraw, ORBIT_BODY_COUNT> table{};
            for(u32 b=0; b<ORBIT_BODY_COUNT; ++b){
                table[b] = OrbitBodyDraw{
                    .colorR = BODY_COLORS[b].x,
                    .colorG = BODY_COLORS[b].y,
                    .colorB = BODY_COLORS[b].z,
                    .trailPeriodDays = static_cast<f32>(TrailPeriodDays(b))
                };
            }
            trailArgs = std::make_unique<OrbitTrailArgs>(device, table);

            uiRenderer = std::make_unique<UIRenderer>(
                device,
                swapchain.GetFormat()
            );

        }

        // The panel is raw ImGui rather than the declarative Widget tree.
        // Widget's Slider and Checkbox take their value once at build time and
        // expose no getter, so every control that can also move from outside -
        // the keys below, a Reset - needs the whole tree rebuilt to show it.
        // Calling ImGui directly makes each control two-way for free, and
        // brings real logarithmic sliders with it.
        //
        // UIRenderer still gets its own window; this sample just leaves it
        // empty, because Prepare hardcodes ImGui::Begin("Crowy") and offers no
        // way to size, place or name it.
        void DrawPanel(){
            ImGui::SetNextWindowPos(ImVec2(12.0f, 6.0f), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(352.0f, 708.0f), ImGuiCond_FirstUseEver);
            ImGui::Begin("OrbitFrameSample");
            // leave room for the label, which ImGui draws to the right of the
            // control and happily clips against the window edge
            ImGui::PushItemWidth(-132.0f);

            ImGui::SliderFloat("heliocentricity", &context.alpha, 0.0f, 1.0f);
            ImGui::TextUnformatted("0 geocentric  <->  1 heliocentric");
            ImGui::SliderFloat("frame lock", &context.frameLock, 0.0f, 1.0f);

            ImGui::SeparatorText("Simulation");
            ImGui::SliderFloat(
                "days / s", &context.timeScale,
                TIME_SCALE_MIN, TIME_SCALE_MAX,
                "%.0f", ImGuiSliderFlags_Logarithmic
            );
            ImGui::Checkbox("paused", &context.paused);
            ImGui::SameLine();
            if(ImGui::Button("Reset to J2000")){
                // safe to act on immediately: the panel is drawn before this
                // frame touches the trail at all
                trail->Prefill(START_DAY);
                pendingSeconds = 0.0;
            }

            ImGui::SeparatorText("View");
            ImGui::SliderFloat(
                "zoom", &context.zoomLog,
                ZOOM_LOG_MIN, ZOOM_LOG_MAX,
                "%.2f"
            );
            ImGui::SliderFloat(
                "orbit turns", &context.orbitTurns,
                TURNS_MIN, TURNS_MAX,
                "%.2f", ImGuiSliderFlags_Logarithmic
            );
            ImGui::SliderFloat(
                "trail px", &context.thicknessPx,
                THICKNESS_MIN, THICKNESS_MAX,
                "%.1f"
            );

            ImGui::SeparatorText("Bodies");
            ImGui::Checkbox("asteroid belt", &context.beltEnabled);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90.0f);
            ImGui::SliderFloat("##beltAlpha", &context.beltAlpha, 0.05f, 1.0f, "%.2f");
            for(u32 b=0; b<ORBIT_BODY_COUNT; ++b){
                if(b % 3 != 0)
                    ImGui::SameLine(static_cast<f32>(b % 3) * 105.0f);

                // the tick takes the body's own trail colour, so the legend and
                // the picture agree without a separate swatch
                ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4{
                    BODY_COLORS[b].x, BODY_COLORS[b].y, BODY_COLORS[b].z, 1.0f
                });
                ImGui::Checkbox(ORBIT_BODY_NAMES[b], &context.bodyEnabled[b]);
                ImGui::PopStyleColor();
            }

            ImGui::Separator();
            ImGui::TextDisabled(
                "keys  up/dn zoom  l/r turns  space  R"
            );

            ImGui::SeparatorText("Grid");
            // the rings carry no world-space labels: the UI layer draws into
            // one window and hands out no draw list, so there is nowhere to put
            // text at an arbitrary screen point. The step is the label instead.
            ImGui::TextUnformatted("thin 1 step, thick 5, plus 1 AU");
            ImGui::Text("step        %.3g AU", context.gridStepAU);
            ImGui::Text("half-height %.3f", context.halfHeight);

            ImGui::SeparatorText("Solver");
            // Both paths write the same positions, so this can be flipped mid
            // flight and the picture must not move. What changes is where the
            // Newton solve runs: on the CPU it is 2400 asteroids every frame
            // plus 65536 * 9 on every reset, on the GPU it is a mean anomaly
            // each and a dispatch.
            if(ImGui::Checkbox("kepler on gpu", &context.gpuFill)){
                trail->SetFillMode(context.gpuFill ?
                    OrbitFillMode::Gpu :
                    OrbitFillMode::Cpu
                );
            }

            ImGui::SeparatorText("Stats");
            ImGui::Text("sim day     %.0f", context.simDay);
            ImGui::Text("samples     %u", context.filled);
            ImGui::Text("draws       %u indirect", ORBIT_BODY_COUNT);
            ImGui::Text("ticks       %llu",
                static_cast<unsigned long long>(context.totalTicks)
            );
            ImGui::Text("ms / frame  %.2f", context.frameTimeMs);

            ImGui::PopItemWidth();
            ImGui::End();
        }

        void ProcessInput(const InputProvider& input) override{
            // the OS layer already drops events ImGui claimed, but IsKeyDown is
            // a level query: a key-up swallowed mid-drag would stick
            if(ImGui::GetIO().WantCaptureKeyboard){
                zoomInput = 0.0f;
                turnsInput = 0.0f;

                return;
            }

            zoomInput =
                (input.IsKeyDown(KeyCode::Up) ? 1.0f : 0.0f) -
                (input.IsKeyDown(KeyCode::Down) ? 1.0f : 0.0f);
            turnsInput =
                (input.IsKeyDown(KeyCode::Right) ? 1.0f : 0.0f) -
                (input.IsKeyDown(KeyCode::Left) ? 1.0f : 0.0f);

            if(input.IsKeyPressed(KeyCode::Space))
                context.paused = !context.paused;
            // the panel reads these live every frame, so a key moving them
            // needs nothing else - which is the whole reason the panel is not
            // a Widget tree
            resetPressed = input.IsKeyPressed(KeyCode::R);
        }

        void OnUpdate(f64 deltaTime, f64) override{
            // the sim step is consumed in OnRecord, where the copy that pairs
            // with it is recorded - OrbitTrail wants exactly one Advance per
            // Record and this is the only way to guarantee it
            pendingSeconds += deltaTime;

            const auto dt = static_cast<f32>(deltaTime);
            // zooming in means a smaller half-height, so Up shrinks it
            context.zoomLog = std::clamp(
                context.zoomLog - zoomInput * ZOOM_SPEED * dt,
                ZOOM_LOG_MIN,
                ZOOM_LOG_MAX
            );
            // proportional, so the key feels the same at either end of a range
            // that spans a factor of 300
            context.orbitTurns = std::clamp(
                context.orbitTurns * std::exp(turnsInput * TURNS_SPEED * dt),
                TURNS_MIN,
                TURNS_MAX
            );
            pendingSeconds += deltaTime;

            context.frameTimeMs = 1000.0 * deltaTime;
            context.halfHeight = std::exp(context.zoomLog);
            context.gridStepAU = GridBaseStepAU();
        }

        Mat4 ViewProj() const{
            const auto halfHeight = std::exp(context.zoomLog);
            const auto aspect = viewportWidth / viewportHeight;

            const auto proj = orthographic(
                2.0f * halfHeight * aspect,
                2.0f * halfHeight,
                NEAR_Z,
                FAR_Z
            );
            // straight down the ecliptic normal, no rotation: +X right,
            // +Y up, and the ecliptic is the screen
            const auto view = viewMat(
                Vec3{0.0f, 0.0f, -CAMERA_DISTANCE},
                Vec4{0.0f, 0.0f, 0.0f, 1.0f}
            );

            return proj * view;
        }

        // -frameLock times Earth's unwrapped ecliptic longitude at the newest
        // sample, reduced to (-pi, pi].
        //
        // The unwrapping has to happen here, in double, because the shader only
        // ever sees a wrapped atan2 and a sim day too big for a float once the
        // run is a few hours old. Earth's mean longitude is exactly linear in
        // time and therefore trivially unwrappable; the true longitude is that
        // plus an equation of centre under two degrees, which comes back out of
        // a normalize. Reducing the result mod 2*pi costs nothing - a whole
        // turn is the identity rotation.
        f32 FrameLockBase() const{
            if(context.frameLock <= 0.0f)
                return 0.0f;

            const auto& earth = ORBIT_ELEMENTS[ORBIT_EARTH_INDEX];
            const auto day = trail->NewestDay();

            const auto meanLon = earth.L0 + MeanMotionDegPerDay(earth) * day;
            const auto p = OrbitPosition(ORBIT_EARTH_INDEX, day);
            const auto trueLon = std::atan2(p.y, p.x) * 180.0 / std::numbers::pi;

            const auto unwrapped = meanLon + NormalizeDegrees(trueLon - meanLon);
            const auto radians = -context.frameLock * unwrapped *
                std::numbers::pi / 180.0;

            return static_cast<f32>(
                NormalizeDegrees(radians * 180.0 / std::numbers::pi) *
                std::numbers::pi / 180.0
            );
        }

        static f32 EarthRadPerDay(){
            return static_cast<f32>(
                MeanMotionDegPerDay(ORBIT_ELEMENTS[ORBIT_EARTH_INDEX]) *
                std::numbers::pi / 180.0
            );
        }

        // screen pixels per unit of warped radius
        f32 PxPerWarpUnit() const{
            return 0.5f * viewportHeight / std::exp(context.zoomLog);
        }

        // The step of the finest ring level, quantized to a power of five AU.
        // Solving step * pxPerWarp == GRID_TARGET_GAP_PX for the step and
        // rounding in log-5 keeps it a number worth printing in the panel.
        f32 GridBaseStepAU() const{
            const auto wanted = GRID_TARGET_GAP_PX / PxPerWarpUnit();
            const auto level = std::round(std::log(wanted) / std::log(5.0f));

            return std::pow(5.0f, level);
        }

        GridPush MakeGridPush() const{
            const auto pxPerWarp = PxPerWarpUnit();

            // the screen corner in warped units, turned back into AU, so the
            // spokes reach past the far corner instead of stopping somewhere
            // inside the view
            const auto halfHeight = std::exp(context.zoomLog);
            const auto cornerWarped = halfHeight * std::hypot(
                viewportWidth / viewportHeight,
                1.0f
            );

            return GridPush{
                .viewportX = viewportWidth,
                .viewportY = viewportHeight,
                .thinThicknessPx = 1.0f,
                .thickThicknessPx = 1.6f,
                .colorR = 0.45f, .colorG = 0.55f, .colorB = 0.70f,
                .thinAlpha = 0.16f,
                .thickAlpha = 0.30f,
                .baseStepAU = GridBaseStepAU(),
                .pxPerWarpUnit = pxPerWarp,
                .spokeRadiusAU = std::expm1(cornerWarped),
                .minGapPx = GRID_MIN_GAP_PX,
                .fadeGapPx = GRID_FADE_GAP_PX,
                .referenceAlpha = 0.5f
            };
        }

        // One bit per body, so a toggle flipped in the panel this frame lands
        // in this frame's draw arguments rather than the next one.
        u32 EnabledMask() const{
            u32 mask = 0;
            for(u32 b=0; b<ORBIT_BODY_COUNT; ++b){
                if(context.bodyEnabled[b])
                    mask |= 1u << b;
            }

            return mask;
        }

        // the GPU path owns its buffer outright, so its rows start at 0;
        // the CPU path rides a slice of the shared transient storage
        RHIBufferSlice BeltPositions() const{
            if(context.gpuFill){
                return RHIBufferSlice{
                    .buffer = beltGpuBuffer.get(),
                    .offset = 0,
                    .size = beltGpuBuffer->GetSize()
                };
            }

            return beltSlice;
        }

        // Every asteroid, every frame - the belt keeps no history, so its whole
        // state is recomputed from the elements each time. That makes it the
        // clearest case for the compute path: a couple of thousand Newton
        // solves and six trigonometric functions each, or a couple of thousand
        // mean anomalies and a dispatch.
        void UpdateBelt(){
            if(context.gpuFill){
                // the epoch is the entire handoff; RecordBelt turns it into one
                // dispatch. Nothing touches the CPU-write buffer this frame,
                // and nothing hands out its descriptor either
                beltFill->SetEpoch(trail->NewestDay(), DAY_PER_SAMPLE);

                return;
            }

            // The solve is skipped when the belt is hidden, but the upload is
            // not: every trail draw carries the buffer's descriptor whether or
            // not the belt is drawn, and a CPUWrite slot that was never written
            // in this frame trips a debug assert the moment one is handed out.
            if(context.beltEnabled)
                belt.Sample(trail->NewestDay(), beltScratch);

            beltSlice = Device().UploadTransient(
                std::span<const Vec3>(beltScratch),
                static_cast<u32>(sizeof(Vec3))
            );
        }

        // Its own compute pass rather than the trail's: the trail only fills on
        // frames that ticked, and the belt has to move every frame.
        std::optional<RHIBufferBarrier> RecordBelt(RHICommandList& cmdList){
            if(!context.gpuFill)
                return std::nullopt;

            const auto acquire = beltResting == RHIResourceUsage::Undefined ?
                MakeBarrier(*beltGpuBuffer,
                    RHIResourceUsage::Undefined,
                    RHIResourceUsage::StorageCompute
                ) :
                MakeCrossSubmissionBarrier(*beltGpuBuffer,
                    beltResting,
                    RHIResourceUsage::StorageCompute
                );
            const auto release = MakeBarrier(*beltGpuBuffer,
                RHIResourceUsage::StorageCompute,
                RHIResourceUsage::SampledVertex
            );

            const std::array acquires{acquire};
            const std::array releases{release};

            cmdList.BeginComputePass({}, acquires);
            beltFill->RecordPoints(cmdList, *beltGpuBuffer);
            cmdList.EndComputePass({}, releases);

            beltResting = RHIResourceUsage::SampledVertex;

            return release;
        }

        void OnRecord(RHICommandList& cmdList, const RHIColorAttachment& backBuffer) override{
            // stats first, then the panel, then everything that reads a
            // control: drawn this early, a slider dragged this frame lands in
            // this frame's picture rather than the next one
            context.filled = trail->Filled();
            context.totalTicks = trail->Stats().totalTicks;
            context.simDay = trail->NewestDay();
            DrawPanel();

            if(std::exchange(resetPressed, false)){
                trail->Prefill(START_DAY);
                pendingSeconds = 0.0;
            }

            if(!context.paused){
                trail->Advance(std::exchange(pendingSeconds, 0.0), context.timeScale);
            }
            else{
                pendingSeconds = 0.0;
            }

            // read off the backbuffer rather than tracked through OnResize:
            // that event only fires on an actual resize, never at startup
            viewportWidth = static_cast<f32>(backBuffer.texture->GetWidth());
            viewportHeight = static_cast<f32>(backBuffer.texture->GetHeight());

            UpdateBelt();

            frameCB = Device().UploadTransient(FrameUniforms{.viewProj = ViewProj()});

            // outside any pass: the copies are their own blit pass
            const auto trailEdge = trail->Record(cmdList);
            const auto beltEdge = RecordBelt(cmdList);
            // the counts the draws below run on, written by the GPU for itself
            const auto argsEdges = trailArgs->Record(
                cmdList,
                trail->Filled(),
                context.orbitTurns,
                static_cast<f32>(DAY_PER_SAMPLE),
                EnabledMask()
            );

            // Prepare opens its own window unconditionally, and its saved
            // position lands right on top of the panel. SetNextWindow* applies
            // to whatever Begin comes next, so the empty one can still be
            // pushed into a corner from out here.
            ImGui::SetNextWindowPos(
                ImVec2(viewportWidth - 168.0f, 8.0f), ImGuiCond_Always
            );
            ImGui::SetNextWindowSize(ImVec2(160.0f, 0.0f), ImGuiCond_Always);
            ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);

            // closes out the ImGui frame DrawPanel wrote into and uploads its
            // geometry
            uiRenderer->Prepare(cmdList, emptyPanel, context);

            auto colorAttachment = backBuffer;
            colorAttachment.clearColor = BACKGROUND;
            std::array colorAttachments = {colorAttachment};

            std::vector<RHITextureBarrier> textureAcquires{
                AcquireBackBuffer(backBuffer)
            };
            // the pass samples the font atlas Prepare just refreshed
            textureAcquires.append_range(uiRenderer->TextureAcquires());

            // only when something was actually copied; an idle frame leaves the
            // ring already resting where the vertex stage wants it
            std::vector<RHIBufferBarrier> bufferAcquires;
            if(trailEdge.has_value())
                bufferAcquires.push_back(*trailEdge);
            if(beltEdge.has_value())
                bufferAcquires.push_back(*beltEdge);
            // the counts the vertex stage fades against, and the arguments
            // the ExecuteIndirect itself reads
            bufferAcquires.push_back(argsEdges.segCounts);
            bufferAcquires.push_back(argsEdges.args);

            cmdList.BeginRenderPass(RHIRenderPassDesc{
                .colorAttachments = colorAttachments
            }, textureAcquires, bufferAcquires);
            cmdList.SetViewport(FullViewport(*backBuffer.texture));
            cmdList.SetScissorRect(FullScissorRect(*backBuffer.texture));

            cmdList.SetGraphicsConstantBuffer(frameCB, 0);

            // no depth buffer: order is the layering. Grid behind, trails over
            // it, markers on top of their own heads.
            cmdList.SetPipelineState(*gridPSO);
            cmdList.SetPushGraphicsConstants(MakeGridPush());
            cmdList.Draw(
                4,
                GRID_LEVEL_COUNT * GRID_RINGS_PER_LEVEL * GRID_RING_SEGMENTS,
                0,
                GRID_DRAW_RINGS
            );
            cmdList.Draw(4, GRID_SPOKE_COUNT, 0, GRID_DRAW_SPOKES);
            cmdList.Draw(4, GRID_RING_SEGMENTS, 0, GRID_DRAW_REFERENCE);

            const auto beltPositions = BeltPositions();
            const TrailPush trailPush{
                .samples = trail->Buffer().GetReadableID(
                    static_cast<u32>(sizeof(Vec3))
                ),
                .bodies = trailArgs->Bodies().GetReadableID(
                    static_cast<u32>(sizeof(OrbitBodyDraw))
                ),
                .viewportX = viewportWidth,
                .viewportY = viewportHeight,
                .thicknessPx = context.thicknessPx,
                .alpha = context.alpha,
                .head = trail->Head(),
                .capacity = trail->Capacity(),
                .markerRadiusPx = MARKER_RADIUS_PX,
                .frameLock = context.frameLock,
                .frameLockBase = FrameLockBase(),
                .earthRadPerDay = EarthRadPerDay(),
                .dayPerSample = static_cast<f32>(DAY_PER_SAMPLE),
                .beltRadiusPx = BELT_RADIUS_PX,
                .belt = beltPositions.buffer->GetReadableID(
                    static_cast<u32>(sizeof(Vec3))
                ),
                .beltAlpha = context.beltAlpha,
                .beltBase = beltPositions.offset / static_cast<u32>(sizeof(Vec3)),
                .segCounts = trailArgs->SegCounts().GetReadableID(
                    static_cast<u32>(sizeof(u32))
                )
            };

            // Nine draws, no loop and no counts on this side. ExecuteIndirect
            // binds the PSO itself; the push constants set before it survive,
            // the root signature being bound once per command list.
            //
            // What made the swap free is that the shader already read its body
            // index off the draw rather than off a uniform - a per-draw push
            // constant would have made this impossible.
            cmdList.SetPushGraphicsConstants(trailPush);
            cmdList.ExecuteIndirect(DrawBatch{
                .pso = trailPSO.get(),
                .args = &trailArgs->Args(),
                .drawCount = trailArgs->DrawCount()
            });

            // under the markers, over the trails: a couple of thousand dots
            // should not cover a planet's head
            if(context.beltEnabled){
                cmdList.SetPipelineState(*beltPSO);
                cmdList.SetPushGraphicsConstants(trailPush);
                cmdList.Draw(4, static_cast<u32>(belt.Count()), 0, 0);
            }

            // one instance per body, so the instance index is the body index -
            // there is no per-draw table to look up here
            cmdList.SetPipelineState(*markerPSO);
            cmdList.SetPushGraphicsConstants(trailPush);
            cmdList.Draw(4, ORBIT_BODY_COUNT, 0, 0);

            uiRenderer->Record(cmdList);

            const std::array releases{ReleaseBackBuffer(backBuffer)};
            cmdList.EndRenderPass(releases);
        }
    };
}

int main(void){
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "OrbitFrameSample",
        .width = 1280, .height = 720,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<OrbitFrameSample>(windowConfig);
}
