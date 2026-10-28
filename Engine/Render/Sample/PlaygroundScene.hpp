#pragma once

#include <array>
#include <optional>
#include <span>
#include <vector>

#include "CommandPort.hpp"
#include "EnumUtil.hpp"
#include "FlyCamera.hpp"
#include "InputProvider.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"
#include "RenderApp.hpp"
#include "StatsOverlay.hpp"
#include "UIRenderer.hpp"
#include "World.hpp"

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

    // clang-format off
    CROWY_ENUM_BEGIN(CameraBookmark)
        CROWY_ENUM_VALUE(Free)
        CROWY_ENUM_VALUE(Overview)
        CROWY_ENUM_VALUE(Scale)
        CROWY_ENUM_VALUE(Emissive)
        CROWY_ENUM_VALUE(Materials)
        CROWY_ENUM_VALUE(Glass)
        CROWY_ENUM_VALUE(DoubleSided)
    CROWY_ENUM_END()
    // clang-format on

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

    // clang-format off
    CROWY_ENUM_BEGIN(ChartShading)
        CROWY_ENUM_VALUE(PBR)
        CROWY_ENUM_VALUE(Toon)
        CROWY_ENUM_VALUE(Unlit)
    CROWY_ENUM_END()
    // clang-format on

    // exposed as `shading`; writing `chart` relinks the chart's materials
    struct PlaygroundShading {
        ChartShading chart = ChartShading::PBR;
    };

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

    // in metres, every box face on the 0.25 m grid the floor draws; nothing
    // floats
    class PlaygroundScene: public RenderApp {
    private:
        // the line every station's front stands on
        static constexpr f32 FrontZ = 3.5f;
        static constexpr f32 SphereRadius = 0.25f;
        // one color for both rows, so metallic is all that differs; an
        // orange, which a metal's highlight takes
        static constexpr Vec3 ChartAlbedo{0.85f, 0.55f, 0.30f};

        // Godot's editor-preview sky; the tone map gives back its bytes
        // (98, 116, 140), 0.4 of a step clear of every rounding edge
        static constexpr Color SkyRadiance{0.139f, 0.212f, 0.356f, 1.0f};

        static constexpr CStr StandardForward =
            "Engine/Render/Shader/StandardForward.slang";

        static constexpr auto PanelToggleKey = KeyCode::P;
        static constexpr auto StatsToggleKey = KeyCode::I;

        // a panel section and a port target at once; the port's write also
        // rebuilds the panel, whose widgets it went behind
        struct InspectedTarget {
            Str portName;
            Str label;
            PropertyTarget target;
            DirtyCallback onChange;
        };

        struct CameraPose {
            Vec3 position;
            f32 yaw = 0.0f;
            f32 pitch = 0.0f;
        };

    private:
        // the map's primitives as actors; the RenderScene rows are its
        World world;

        GeometryAllocation sphere{};
        // stretched into walls, boards and bars, scaled into blocks
        MeshAssetHandle unitBox;
        GeometryAllocation floorPlane{};
        // unit quads; the one facing away only shows where nothing is culled
        GeometryAllocation quadFacingCamera{};
        GeometryAllocation quadFacingAway{};

        RAII<UIRenderer> uiRenderer;
        UIContext uiContext;
        Widget panel = Column({});
        std::vector<InspectedTarget> inspected;
        // what the panel's debug section was built from
        RenderDebug shownDebug;
        CameraBookmarks bookmarks;
        PlaygroundShading shading;
        // the actor the map is seen through, flown by the mouse and the keys
        ActorId cameraActor;
        FlyState fly;
        PlaygroundMaterials materials;

        // shown by debug.showStats, hidden by default like the panel so the
        // smoke capture matches the panel-less one
        StatsOverlay statsOverlay;
        // last, so the port lets go of the members above before they go
        PortExposures exposures;

    public:
        ~PlaygroundScene() override = default;
        CROWY_DECLARE_PINNED(PlaygroundScene)

        explicit PlaygroundScene(const Config& config = MakeConfig());

    protected:
        static Config MakeConfig();
        // the start pose is the overview bookmark, which `current` starts on
        static FlyCamera::Config MakeCamera();

        // after every material row is added; `shading` is what the toggle
        // starts on
        virtual void OnRestyleMaterials(
            RenderScene&,
            const PlaygroundMaterials&,
            PlaygroundShading&
        ) {}

        void OnBuildGeometry(GeometryPool& pool) override;
        void ExtractScene(RenderScene& scene) override;
        void OnProcessInput(const InputProvider& input) override;
        void OnUpdateScene(f64 seconds) override;
        void OnInitUI(
            RHIDevice& device,
            const OverlayFormats& formats
        ) override;
        std::span<const RHITextureBarrier> OnPrepareUI(
            RHICommandList& cmdList
        ) override;
        void OnRecordUI(RHICommandList& cmdList) override;

    private:
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
        static MeshHandle addMesh(
            RenderScene& scene,
            const MeshAssetHandle& asset,
            MaterialHandle material
        );
        // both stages from one file, so the prepass and the color pass run
        // the same vertex shader; every material links the default PBR
        static MaterialPipelineDesc basePipeline(CStr file, CStr fragmentEntry);
        static MaterialPipelineDesc gridPipeline();
        static MaterialPipelineDesc opaquePipeline();
        static MaterialPipelineDesc doubleSidedPipeline();
        static MaterialPipelineDesc translucentPipeline();

        // an actor drawing the mesh, translated and scaled only
        void addPrimitive(MeshHandle mesh, Vec3 position, Vec3 scale);

        Widget buildPanel();
        // the row's data by its handle, for the panel and the port alike
        PropertyTarget materialTarget(MaterialHandle handle);
        Widget debugSection();
        // the panel's sections after debug, which are the port's targets
        void inspect(RenderScene& scene, ActorId sun, ActorId lamp);
        void linkChartShading();
        // a snap changes the camera section too, so the panel rebuilds; from
        // inside the panel's own submit this only raises the flag
        void snapCamera();
        // the look into the camera actor's rotation
        void applyLook();
    };
}
