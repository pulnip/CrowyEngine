#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <utility>
#include <vector>

#include <imgui.h>

#include "AppFramework.hpp"
#include "AsteroidBelt.hpp"
#include "InputProvider.hpp"
#include "Kepler.hpp"
#include "LinearAlgebra.hpp"
#include "OrbitDrawArgs.hpp"
#include "OrbitTrail.hpp"
#include "RHIBuffer.hpp"
#include "RHIPipelineState.hpp"
#include "UIRenderer.hpp"

namespace Crowy
{
    // Named UIContext because UIRenderer::Prepare takes one; the panel is raw
    // ImGui.
    struct UIContext {
        // 0 geocentric, 1 heliocentric
        f32 alpha = 0.0f;
        f32 frameLock = 0.0f;

        f32 timeScale = 120.0f;
        f32 zoomLog = 1.34f;
        f32 orbitTurns = 1.0f;
        f32 thicknessPx = 2.0f;

        std::array<bool, ORBIT_BODY_COUNT> bodyEnabled{};
        bool beltEnabled = true;
        bool gpuFill = true;
        f32 beltAlpha = 0.55f;
        bool paused = false;

        f32 halfHeight = 1.0f;
        f32 gridStepAU = 1.0f;
        f64 frameTimeMs = 0.0;
        f64 simDay = 0.0;
        u32 filled = 0;
        u64 totalTicks = 0;

        UIContext() { bodyEnabled.fill(true); }
    };

    // The origin slides between Earth and the Sun in the shader, not the
    // camera, so at 0 the heliocentric ellipses come apart into Ptolemy's
    // epicycles.
    class OrbitFrame: public App {
        using App::App;

        static constexpr f64 DAY_PER_SAMPLE = 1.0;
        static constexpr f64 START_DAY = 0.0;

        static constexpr Color BACKGROUND{0.02f, 0.02f, 0.05f, 1.0f};

        // zoomed out Neptune fills two thirds of the view, zoomed in Mercury
        // fills it
        static constexpr f32 ZOOM_LOG_MIN = -1.2f;
        static constexpr f32 ZOOM_LOG_MAX = 1.7f;

        // Venus's rose closes after 13 Venus years (8 Earth years)
        static constexpr f32 TURNS_MIN = 0.05f, TURNS_MAX = 15.0f;

        static constexpr f32 TIME_SCALE_MIN = 1.0f, TIME_SCALE_MAX = 2000.0f;
        static constexpr f32 THICKNESS_MIN = 1.0f, THICKNESS_MAX = 6.0f;

        static constexpr f32 CAMERA_DISTANCE = 100.0f;
        static constexpr f32 NEAR_Z = 1.0f, FAR_Z = 200.0f;

        // mirrors TrailPush in OrbitTrail.slang
        struct TrailPush {
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

        // mirrors GridPush in OrbitGrid.slang
        struct GridPush {
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

        // mirrors OrbitGrid.slang
        static constexpr u32 GRID_LEVEL_COUNT = 8;
        static constexpr u32 GRID_RINGS_PER_LEVEL = 16;
        static constexpr u32 GRID_RING_SEGMENTS = 192;
        static constexpr u32 GRID_SPOKE_COUNT = 12;
        static constexpr u32 GRID_DRAW_RINGS = 0;
        static constexpr u32 GRID_DRAW_SPOKES = 1;
        static constexpr u32 GRID_DRAW_REFERENCE = 2;

        static constexpr f32 GRID_TARGET_GAP_PX = 100.0f;
        static constexpr f32 GRID_MIN_GAP_PX = 3.5f;
        static constexpr f32 GRID_FADE_GAP_PX = 11.0f;

        static constexpr f32 MARKER_RADIUS_PX = 4.0f;
        static constexpr f32 BELT_RADIUS_PX = 1.3f;

        struct FrameUniforms {
            Mat4 viewProj;
        };

        static_assert(sizeof(Vec3) == 12);

        // the Sun is not yellow: at alpha 0 it takes over Earth's ring
        static constexpr std::array<Vec3, ORBIT_BODY_COUNT> BODY_COLORS{
            Vec3{1.00f, 0.95f, 0.70f}, // Sun
            Vec3{0.78f, 0.72f, 0.66f}, // Mercury
            Vec3{0.95f, 0.72f, 0.40f}, // Venus
            Vec3{0.40f, 0.72f, 1.00f}, // Earth
            Vec3{0.94f, 0.42f, 0.32f}, // Mars
            Vec3{0.90f, 0.76f, 0.55f}, // Jupiter
            Vec3{0.85f, 0.80f, 0.55f}, // Saturn
            Vec3{0.60f, 0.88f, 0.90f}, // Uranus
            Vec3{0.42f, 0.55f, 0.95f}  // Neptune
        };

        RHIDevice* device = nullptr;
        RAII<OrbitTrail> trail;
        AsteroidBelt belt;
        std::vector<Vec3> beltScratch;

        RHIGraphicsPipelineStateRAII trailPSO, gridPSO, markerPSO, beltPSO;
        RHIBufferSlice frameCB;
        RAII<OrbitTrailArgs> trailArgs;
        RHIBufferSlice beltSlice;
        // the compute path cannot write a CPU-write buffer
        RHIBufferRAII beltGpuBuffer;
        RAII<OrbitKeplerFill> beltFill;
        RHIResourceUsage beltResting = RHIResourceUsage::Undefined;

        RAII<UIRenderer> uiRenderer;
        UIContext context;
        Widget emptyPanel = Column({});

        f64 pendingSeconds = 0.0;
        f32 viewportWidth = 1.0f, viewportHeight = 1.0f;

        f32 zoomInput = 0.0f, turnsInput = 0.0f;
        bool resetPressed = false;

        static constexpr f32 ZOOM_SPEED = 1.2f;
        static constexpr f32 TURNS_SPEED = 1.5f;

        // unculled: an expanded quad's winding follows the direction it was
        // expanded along
        static RHIGraphicsPipelineStateDesc LineExpansionDesc(
            CStr path,
            CStr vertexEntry,
            CStr fragmentEntry,
            RHIPixelFormat format
        ) {
            return RHIGraphicsPipelineStateDesc{
                .topology = RHIPrimitiveTopology::TriangleStrip,
                .vertexShader = {.path = path, .entryPoint = vertexEntry},
                .rasterizer = RHIRasterizerState{.cullMode = RHICullMode::None},
                .fragmentShader =
                    RHIShaderDesc{.path = path, .entryPoint = fragmentEntry},
                .blend =
                    RHIBlendState{
                        .renderTargets = {RHIRenderTargetBlendState{
                            .blendEnable = true,
                            .srcBlend = RHIBlend::SrcAlpha,
                            .dstBlend = RHIBlend::InvSrcAlpha,
                            .blendOp = RHIBlendOp::Add,
                            .srcBlendAlpha = RHIBlend::One,
                            .dstBlendAlpha = RHIBlend::InvSrcAlpha,
                            .blendOpAlpha = RHIBlendOp::Add
                        }}
                    },
                .renderTargetFormats = {format},
                .renderTargetCount = 1,
                .profile = "sm_6_8"
            };
        }

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override {
            this->device = &device;

            trail = std::make_unique<OrbitTrail>(
                device,
                DAY_PER_SAMPLE,
                ORBIT_TRAIL_CAPACITY,
                RHIResourceUsage::SampledVertex
            );
            trail->SetFillMode(
                context.gpuFill ? OrbitFillMode::Gpu : OrbitFillMode::Cpu
            );
            trail->Prefill(START_DAY);

            trailPSO = device.CreatePipelineState(
                LineExpansionDesc(
                    "Engine/RHI/Sample/Orbit/OrbitTrail.slang",
                    "vs_main",
                    "fs_main",
                    swapchain.GetFormat()
                ),
                "OrbitTrailPSO"
            );
            gridPSO = device.CreatePipelineState(
                LineExpansionDesc(
                    "Engine/RHI/Sample/Orbit/OrbitGrid.slang",
                    "vs_main",
                    "fs_main",
                    swapchain.GetFormat()
                ),
                "OrbitGridPSO"
            );
            markerPSO = device.CreatePipelineState(
                LineExpansionDesc(
                    "Engine/RHI/Sample/Orbit/OrbitTrail.slang",
                    "vs_marker",
                    "fs_marker",
                    swapchain.GetFormat()
                ),
                "OrbitMarkerPSO"
            );
            beltPSO = device.CreatePipelineState(
                LineExpansionDesc(
                    "Engine/RHI/Sample/Orbit/OrbitTrail.slang",
                    "vs_belt",
                    "fs_marker",
                    swapchain.GetFormat()
                ),
                "OrbitBeltPSO"
            );

            beltScratch.resize(belt.Count());

            beltGpuBuffer = device.CreateBuffer(
                RHIBufferCreateDesc{
                    .size = static_cast<u32>(belt.Count() * sizeof(Vec3)),
                    .shaderWrite = true
                },
                "OrbitAsteroidBeltGPU"
            );
            beltFill =
                std::make_unique<OrbitKeplerFill>(device, belt.Elements());

            std::array<OrbitBodyDraw, ORBIT_BODY_COUNT> table{};
            for(u32 b = 0; b < ORBIT_BODY_COUNT; ++b) {
                table[b] = OrbitBodyDraw{
                    .colorR = BODY_COLORS[b].x,
                    .colorG = BODY_COLORS[b].y,
                    .colorB = BODY_COLORS[b].z,
                    .trailPeriodDays = static_cast<f32>(TrailPeriodDays(b))
                };
            }
            trailArgs = std::make_unique<OrbitTrailArgs>(device, table);

            uiRenderer =
                std::make_unique<UIRenderer>(device, swapchain.GetFormat());
        }

        // Raw ImGui: Widget's Slider and Checkbox take their value once at
        // build time, and the keys move these controls from outside.
        void DrawPanel() {
            ImGui::SetNextWindowPos(
                ImVec2(12.0f, 6.0f),
                ImGuiCond_FirstUseEver
            );
            ImGui::SetNextWindowSize(
                ImVec2(352.0f, 708.0f),
                ImGuiCond_FirstUseEver
            );
            ImGui::Begin("OrbitFrame");
            // ImGui draws the label right of the control and clips it at the
            // window edge
            ImGui::PushItemWidth(-132.0f);

            ImGui::SliderFloat("heliocentricity", &context.alpha, 0.0f, 1.0f);
            ImGui::TextUnformatted("0 geocentric  <->  1 heliocentric");
            ImGui::SliderFloat("frame lock", &context.frameLock, 0.0f, 1.0f);

            ImGui::SeparatorText("Simulation");
            ImGui::SliderFloat(
                "days / s",
                &context.timeScale,
                TIME_SCALE_MIN,
                TIME_SCALE_MAX,
                "%.0f",
                ImGuiSliderFlags_Logarithmic
            );
            ImGui::Checkbox("paused", &context.paused);
            ImGui::SameLine();
            if(ImGui::Button("Reset to J2000")) {
                trail->Prefill(START_DAY);
                pendingSeconds = 0.0;
            }

            ImGui::SeparatorText("View");
            ImGui::SliderFloat(
                "zoom",
                &context.zoomLog,
                ZOOM_LOG_MIN,
                ZOOM_LOG_MAX,
                "%.2f"
            );
            ImGui::SliderFloat(
                "orbit turns",
                &context.orbitTurns,
                TURNS_MIN,
                TURNS_MAX,
                "%.2f",
                ImGuiSliderFlags_Logarithmic
            );
            ImGui::SliderFloat(
                "trail px",
                &context.thicknessPx,
                THICKNESS_MIN,
                THICKNESS_MAX,
                "%.1f"
            );

            ImGui::SeparatorText("Bodies");
            ImGui::Checkbox("asteroid belt", &context.beltEnabled);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90.0f);
            ImGui::SliderFloat(
                "##beltAlpha",
                &context.beltAlpha,
                0.05f,
                1.0f,
                "%.2f"
            );
            for(u32 b = 0; b < ORBIT_BODY_COUNT; ++b) {
                if(b % 3 != 0)
                    ImGui::SameLine(static_cast<f32>(b % 3) * 105.0f);

                ImGui::PushStyleColor(
                    ImGuiCol_CheckMark,
                    ImVec4{
                        BODY_COLORS[b].x,
                        BODY_COLORS[b].y,
                        BODY_COLORS[b].z,
                        1.0f
                    }
                );
                ImGui::Checkbox(ORBIT_BODY_NAMES[b], &context.bodyEnabled[b]);
                ImGui::PopStyleColor();
            }

            ImGui::Separator();
            ImGui::TextDisabled("keys  up/dn zoom  l/r turns  space  R");

            ImGui::SeparatorText("Grid");
            // UIRenderer hands out no draw list, so the rings carry no labels
            ImGui::TextUnformatted("thin 1 step, thick 5, plus 1 AU");
            ImGui::Text("step        %.3g AU", context.gridStepAU);
            ImGui::Text("half-height %.3f", context.halfHeight);

            ImGui::SeparatorText("Solver");
            if(ImGui::Checkbox("kepler on gpu", &context.gpuFill)) {
                trail->SetFillMode(
                    context.gpuFill ? OrbitFillMode::Gpu : OrbitFillMode::Cpu
                );
            }

            ImGui::SeparatorText("Stats");
            ImGui::Text("sim day     %.0f", context.simDay);
            ImGui::Text("samples     %u", context.filled);
            ImGui::Text("draws       %u indirect", ORBIT_BODY_COUNT);
            ImGui::Text(
                "ticks       %llu",
                static_cast<unsigned long long>(context.totalTicks)
            );
            ImGui::Text("ms / frame  %.2f", context.frameTimeMs);

            ImGui::PopItemWidth();
            ImGui::End();
        }

        void ProcessInput(const InputProvider& input) override {
            zoomInput = (input.IsKeyDown(KeyCode::Up) ? 1.0f : 0.0f) -
                        (input.IsKeyDown(KeyCode::Down) ? 1.0f : 0.0f);
            turnsInput = (input.IsKeyDown(KeyCode::Right) ? 1.0f : 0.0f) -
                         (input.IsKeyDown(KeyCode::Left) ? 1.0f : 0.0f);

            if(input.IsKeyPressed(KeyCode::Space))
                context.paused = !context.paused;
            resetPressed = input.IsKeyPressed(KeyCode::R);
        }

        void OnUpdate(f64 deltaTime, f64) override {
            // OrbitTrail wants exactly one Advance per Record, so OnRecord
            // takes the step
            pendingSeconds += deltaTime;

            const auto dt = static_cast<f32>(deltaTime);
            context.zoomLog = std::clamp(
                context.zoomLog - zoomInput * ZOOM_SPEED * dt,
                ZOOM_LOG_MIN,
                ZOOM_LOG_MAX
            );
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

        Mat4 ViewProj() const {
            const auto halfHeight = std::exp(context.zoomLog);
            const auto aspect = viewportWidth / viewportHeight;

            const auto proj = orthographic(
                2.0f * halfHeight * aspect,
                2.0f * halfHeight,
                NEAR_Z,
                FAR_Z
            );
            const auto view = viewMat(
                Vec3{0.0f, 0.0f, -CAMERA_DISTANCE},
                Vec4{0.0f, 0.0f, 0.0f, 1.0f}
            );

            return proj * view;
        }

        // Unwrapped here in double: the shader sees a wrapped atan2 and a sim
        // day a float cannot hold once the run is a few hours old.
        f32 FrameLockBase() const {
            if(context.frameLock <= 0.0f)
                return 0.0f;

            const auto& earth = ORBIT_ELEMENTS[ORBIT_EARTH_INDEX];
            const auto day = trail->NewestDay();

            const auto meanLon = earth.L0 + MeanMotionDegPerDay(earth) * day;
            const auto p = OrbitPosition(ORBIT_EARTH_INDEX, day);
            const auto trueLon =
                std::atan2(p.y, p.x) * 180.0 / std::numbers::pi;

            const auto unwrapped =
                meanLon + NormalizeDegrees(trueLon - meanLon);
            const auto radians =
                -context.frameLock * unwrapped * std::numbers::pi / 180.0;

            return static_cast<f32>(
                NormalizeDegrees(radians * 180.0 / std::numbers::pi) *
                std::numbers::pi / 180.0
            );
        }

        static f32 EarthRadPerDay() {
            return static_cast<f32>(
                MeanMotionDegPerDay(ORBIT_ELEMENTS[ORBIT_EARTH_INDEX]) *
                std::numbers::pi / 180.0
            );
        }

        f32 PxPerWarpUnit() const {
            return 0.5f * viewportHeight / std::exp(context.zoomLog);
        }

        f32 GridBaseStepAU() const {
            const auto wanted = GRID_TARGET_GAP_PX / PxPerWarpUnit();
            const auto level = std::round(std::log(wanted) / std::log(5.0f));

            return std::pow(5.0f, level);
        }

        GridPush MakeGridPush() const {
            const auto pxPerWarp = PxPerWarpUnit();

            const auto halfHeight = std::exp(context.zoomLog);
            const auto cornerWarped =
                halfHeight * std::hypot(viewportWidth / viewportHeight, 1.0f);

            return GridPush{
                .viewportX = viewportWidth,
                .viewportY = viewportHeight,
                .thinThicknessPx = 1.0f,
                .thickThicknessPx = 1.6f,
                .colorR = 0.45f,
                .colorG = 0.55f,
                .colorB = 0.70f,
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

        u32 EnabledMask() const {
            u32 mask = 0;
            for(u32 b = 0; b < ORBIT_BODY_COUNT; ++b) {
                if(context.bodyEnabled[b])
                    mask |= 1u << b;
            }

            return mask;
        }

        RHIBufferSlice BeltPositions() const {
            if(context.gpuFill) {
                return RHIBufferSlice{
                    .buffer = beltGpuBuffer.get(),
                    .offset = 0,
                    .size = beltGpuBuffer->GetSize()
                };
            }

            return beltSlice;
        }

        void UpdateBelt() {
            if(context.gpuFill) {
                beltFill->SetEpoch(trail->NewestDay(), DAY_PER_SAMPLE);

                return;
            }

            // Uploaded even when hidden: every trail draw carries the
            // descriptor, and handing out an unwritten CPUWrite slot asserts.
            if(context.beltEnabled)
                belt.Sample(trail->NewestDay(), beltScratch);

            beltSlice = Device().UploadTransient(
                std::span<const Vec3>(beltScratch),
                static_cast<u32>(sizeof(Vec3))
            );
        }

        // not the trail's pass: the trail fills only on frames that ticked
        std::optional<RHIBufferBarrier> RecordBelt(RHICommandList& cmdList) {
            if(!context.gpuFill)
                return std::nullopt;

            const auto acquire = beltResting == RHIResourceUsage::Undefined
                                     ? makeBarrier(
                                           *beltGpuBuffer,
                                           RHIResourceUsage::Undefined,
                                           RHIResourceUsage::StorageCompute
                                       )
                                     : makeCrossSubmissionBarrier(
                                           *beltGpuBuffer,
                                           beltResting,
                                           RHIResourceUsage::StorageCompute
                                       );
            const auto release = makeBarrier(
                *beltGpuBuffer,
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

        void OnRecord(
            RHICommandList& cmdList,
            const RHIColorAttachment& backBuffer
        ) override {
            context.filled = trail->Filled();
            context.totalTicks = trail->Stats().totalTicks;
            context.simDay = trail->NewestDay();
            DrawPanel();

            if(std::exchange(resetPressed, false)) {
                trail->Prefill(START_DAY);
                pendingSeconds = 0.0;
            }

            if(!context.paused) {
                trail->Advance(
                    std::exchange(pendingSeconds, 0.0),
                    context.timeScale
                );
            }
            else {
                pendingSeconds = 0.0;
            }

            // OnResize never fires at startup
            viewportWidth = static_cast<f32>(backBuffer.texture->GetWidth());
            viewportHeight = static_cast<f32>(backBuffer.texture->GetHeight());

            UpdateBelt();

            frameCB =
                Device().UploadTransient(FrameUniforms{.viewProj = ViewProj()});

            const auto trailEdge = trail->Record(cmdList);
            const auto beltEdge = RecordBelt(cmdList);
            const auto argsEdges = trailArgs->Record(
                cmdList,
                trail->Filled(),
                context.orbitTurns,
                static_cast<f32>(DAY_PER_SAMPLE),
                EnabledMask()
            );

            // pushes the empty window Prepare opens into a corner
            ImGui::SetNextWindowPos(
                ImVec2(viewportWidth - 168.0f, 8.0f),
                ImGuiCond_Always
            );
            ImGui::SetNextWindowSize(ImVec2(160.0f, 0.0f), ImGuiCond_Always);
            ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);

            uiRenderer->Prepare(cmdList, emptyPanel, context);

            auto colorAttachment = backBuffer;
            colorAttachment.clearColor = BACKGROUND;
            std::array colorAttachments = {colorAttachment};

            std::vector<RHITextureBarrier> textureAcquires{
                AcquireBackBuffer(backBuffer)
            };
            textureAcquires.append_range(uiRenderer->TextureAcquires());

            std::vector<RHIBufferBarrier> bufferAcquires;
            if(trailEdge.has_value())
                bufferAcquires.push_back(*trailEdge);
            if(beltEdge.has_value())
                bufferAcquires.push_back(*beltEdge);
            bufferAcquires.push_back(argsEdges.segCounts);
            bufferAcquires.push_back(argsEdges.args);

            cmdList.BeginRenderPass(
                RHIRenderPassDesc{.colorAttachments = colorAttachments},
                textureAcquires,
                bufferAcquires
            );
            cmdList.SetViewport(fullViewport(*backBuffer.texture));
            cmdList.SetScissorRect(fullScissorRect(*backBuffer.texture));

            cmdList.SetGraphicsConstantBuffer(frameCB, 0);

            // no depth buffer: draw order is the layering
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
                .beltBase =
                    beltPositions.offset / static_cast<u32>(sizeof(Vec3)),
                .segCounts = trailArgs->SegCounts().GetReadableID(
                    static_cast<u32>(sizeof(u32))
                )
            };

            // the push constants survive: the root signature is bound once per
            // command list
            cmdList.SetPushGraphicsConstants(trailPush);
            cmdList.ExecuteIndirect(
                DrawBatch{
                    .pso = trailPSO.get(),
                    .args = &trailArgs->Args(),
                    .drawCount = trailArgs->DrawCount()
                }
            );

            if(context.beltEnabled) {
                cmdList.SetPipelineState(*beltPSO);
                cmdList.SetPushGraphicsConstants(trailPush);
                cmdList.Draw(4, static_cast<u32>(belt.Count()), 0, 0);
            }

            cmdList.SetPipelineState(*markerPSO);
            cmdList.SetPushGraphicsConstants(trailPush);
            cmdList.Draw(4, ORBIT_BODY_COUNT, 0, 0);

            uiRenderer->Record(cmdList);

            const std::array releases{ReleaseBackBuffer(backBuffer)};
            cmdList.EndRenderPass(releases);
        }
    };
}

int main(int argc, char** argv) {
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "OrbitFrame",
        .width = 1280,
        .height = 720,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<OrbitFrame>(argc, argv, windowConfig);
}
