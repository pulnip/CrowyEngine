#pragma once

#include <array>
#include <optional>
#include <span>
#include <vector>

#include "EnumUtil.hpp"
#include "FlyCamera.hpp"
#include "InputProvider.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"
#include "RenderApp.hpp"
#include "StatsOverlay.hpp"
#include "UIRenderer.hpp"

namespace Crowy
{
    // dielectric in front, metal behind: a real surface is one or the other,
    // so metallic takes no values between
    inline constexpr u32 PlaygroundChartRows = 2;
    // roughness 0, 0.25, 0.5, 0.75, 1 from left to right
    inline constexpr u32 PlaygroundChartColumns = 5;

    struct UIContext {
        // a value changed behind the panel's widgets, which seed only when
        // they are built
        bool panelDirty = false;
    };

    // a named camera pose per station; Free leaves the camera where it is
    enum class CameraBookmark : u32 {
        Free,
        Overview,
        Scale,
        Emissive,
        Materials,
        Glass,
        DoubleSided,
    };

    CROWY_ENUM_BEGIN(CameraBookmark)
    CROWY_ENUM_VALUE(Free)
    CROWY_ENUM_VALUE(Overview)
    CROWY_ENUM_VALUE(Scale)
    CROWY_ENUM_VALUE(Emissive)
    CROWY_ENUM_VALUE(Materials)
    CROWY_ENUM_VALUE(Glass)
    CROWY_ENUM_VALUE(DoubleSided)
    CROWY_ENUM_END()

    // exposed as `bookmarks`; writing `current` snaps the camera, even to
    // the bookmark it already names
    struct CameraBookmarks {
        CameraBookmark current = CameraBookmark::Overview;
    };

    // the shading model the chart's ten materials link
    enum class ChartShading : u32 {
        PBR,
        Toon,
        Unlit,
    };

    CROWY_ENUM_BEGIN(ChartShading)
    CROWY_ENUM_VALUE(PBR)
    CROWY_ENUM_VALUE(Toon)
    CROWY_ENUM_VALUE(Unlit)
    CROWY_ENUM_END()

    // exposed as `shading`; writing `chart` relinks the chart's materials
    struct PlaygroundShading {
        ChartShading chart = ChartShading::PBR;
    };

    // the map's material rows by station
    struct PlaygroundMaterials {
        MaterialHandle floor{};
        MaterialHandle wall{};
        // the scale blocks, the chart's step and the emissive niche
        MaterialHandle block{};
        // 1, 4 and 16
        std::array<MaterialHandle, 3> emissive{};
        // front row then back row, roughness rising left to right
        std::array<MaterialHandle, PlaygroundChartRows * PlaygroundChartColumns>
            chart{};
        // red, green and blue behind the glass
        std::array<MaterialHandle, 3> bars{};
        MaterialHandle doubleSided{};
        MaterialHandle translucent{};
    };

    // The prototyping map, in metres: a row of test stations across the
    // back of a walled floor, each testing one thing, fronts on one line,
    // every box face on the 0.25 m grid the floor draws. Nothing floats.
    // Each pass's draw list orders its own draws: opaque ones by pipeline,
    // then near first, the glass far first.
    class PlaygroundScene: public RenderApp {
    private:
        // the line every station's front stands on
        static constexpr f32 FrontZ = 3.5f;
        static constexpr f32 SphereRadius = 0.25f;
        // one colour for both rows, so metallic is all that differs; an
        // orange, which a metal's highlight takes
        static constexpr Vec3 ChartAlbedo{0.85f, 0.55f, 0.30f};

        // Godot's editor-preview sky as radiance: the tone map takes it back
        // to the bytes (98, 116, 140) it was written as, 0.4 of a step clear
        // of every rounding edge
        static constexpr Color SkyRadiance{0.139f, 0.212f, 0.356f, 1.0f};

        // the engine's own colour passes, by path
        static constexpr CStr StandardForward =
            "Engine/Render/Shader/StandardForward.slang";

        static constexpr auto PanelToggleKey = KeyCode::P;
        static constexpr auto StatsToggleKey = KeyCode::I;

        struct NamedMaterial {
            CStr name;
            MaterialHandle handle;
        };

        struct NamedLight {
            CStr name;
            LightHandle handle;
        };

        struct CameraPose {
            Vec3 position;
            f32 yaw = 0.0f;
            f32 pitch = 0.0f;
        };

    private:
        GeometryAllocation sphere{};
        // stretched into walls, boards and bars, scaled into blocks
        GeometryAllocation unitBox{};
        GeometryAllocation floorPlane{};
        // unit quads; the one facing away only shows where nothing is culled
        GeometryAllocation quadFacingCamera{};
        GeometryAllocation quadFacingAway{};

        RAII<UIRenderer> uiRenderer;
        UIContext uiContext;
        Widget panel = Column({});
        // the panel's light and material sections, and the port's targets
        // besides `camera` and `environment`
        std::vector<NamedLight> exposedLights;
        std::vector<NamedMaterial> exposedMaterials;
        // what the panel's debug section was built from
        RenderDebug shownDebug;
        CameraBookmarks bookmarks;
        PlaygroundShading shading;
        PlaygroundMaterials materials;

        // shown by debug.showStats, hidden by default like the panel so the
        // smoke capture matches the panel-less one
        StatsOverlay statsOverlay;

    public:
        // the port outlives these members, and its callbacks point at them
        ~PlaygroundScene() override;
        CROWY_DECLARE_PINNED(PlaygroundScene)

        explicit PlaygroundScene(const Config& config = MakeConfig());

    protected:
        static Config MakeConfig();
        // the start pose is the overview bookmark, which `current` starts on
        static FlyCamera::Config MakeCamera();

        // after every material row is added and before any address is
        // taken; `shading` is what the toggle starts on
        virtual void OnRestyleMaterials(
            RenderScene&,
            const PlaygroundMaterials&,
            PlaygroundShading&
        ) {}

        void OnBuildGeometry(GeometryPool& pool) override;
        void ExtractScene(RenderScene& scene) override;
        void OnProcessInput(const InputProvider& input) override;
        void OnInitUI(
            RHIDevice& device,
            const OverlayFormats& formats
        ) override;
        std::span<const RHITextureBarrier> OnPrepareUI(
            RHICommandList& cmdList
        ) override;
        void OnRecordUI(RHICommandList& cmdList) override;

    private:
        // rows do not move after extraction, so the row's address holds as
        // a write-back target for the panel and the port alike
        static MaterialData& materialData(
            RenderScene& scene,
            MaterialHandle handle
        );
        static CStr moduleOf(ChartShading chart);
        // each close-up frames its station at eye height
        static std::optional<CameraPose> poseOf(CameraBookmark bookmark);
        static MaterialHandle addMaterial(
            RenderScene& scene,
            const MaterialData& data,
            const MaterialPipelineDesc& pipeline
        );
        static MeshHandle addMesh(
            RenderScene& scene,
            const GeometryAllocation& geometry,
            MaterialHandle material,
            Vec3 halfScale
        );
        // translate and scale only, so the world bounds are the local ones
        // scaled per axis
        static void addPrimitive(
            RenderScene& scene,
            MeshHandle mesh,
            Vec3 position,
            Vec3 scale
        );
        // both stages from one file, so the prepass and the colour pass run
        // the same vertex shader; every material links the default PBR
        static MaterialPipelineDesc basePipeline(CStr file, CStr fragmentEntry);
        // the map's own surface: the standard loop plus the grid
        static MaterialPipelineDesc gridPipeline();
        static MaterialPipelineDesc opaquePipeline();
        static MaterialPipelineDesc doubleSidedPipeline();
        static MaterialPipelineDesc translucentPipeline();

        Widget buildPanel();
        Widget materialSection(
            RenderScene& scene,
            CStr label,
            MaterialHandle handle
        );
        Widget lightSection(RenderScene& scene, CStr label, LightHandle handle);
        Widget environmentSection(RenderScene& scene);
        Widget debugSection();
        Widget shadingSection();
        Widget bookmarksSection();
        Widget cameraSection();
        void linkChartShading();
        // a snap changes the camera section too, so the panel rebuilds; from
        // inside the panel's own submit this only raises the flag
        void snapCamera();
    };
}
