#pragma once

#include <array>
#include <optional>
#include <span>
#include <vector>

#include "CommandPort.hpp"
#include "EditorCamera.hpp"
#include "EnumUtil.hpp"
#include "EulerInspector.hpp"
#include "FlyCamera.hpp"
#include "InputProvider.hpp"
#include "LinearAlgebra.hpp"
#include "MaterialAsset.hpp"
#include "PrimitiveActors.hpp"
#include "Primitives.hpp"
#include "RenderApp.hpp"
#include "StatsOverlay.hpp"
#include "UIRenderer.hpp"
#include "World.hpp"
#include "WorldEditor.hpp"

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

    // the scene file's materials, held so the panel, the port and a restyle
    // reach the rows every mesh of that file draws
    struct PlaygroundMaterials {
        MaterialAssetHandle floor;
        MaterialAssetHandle wall;
        // the scale blocks, the chart's step and the emissive niche
        MaterialAssetHandle block;
        // 1, 4 and 16
        std::array<MaterialAssetHandle, 3> emissive;
        // front row then back row, roughness rising left to right; the lanes
        // carry Toon's defaults, which PBR ignores
        std::array<
            MaterialAssetHandle,
            PlaygroundChartRows * PlaygroundChartColumns>
            chart;
        // red, green and blue behind the glass
        std::array<MaterialAssetHandle, 3> bars;
        MaterialAssetHandle doubleSided;
        MaterialAssetHandle translucent;
    };

    // the Scene View's image, window points
    struct SceneImage {
        Vec2 origin;
        Vec2 size;
    };

    // in metres, every box face on the 0.25 m grid the floor draws; nothing
    // floats
    class PlaygroundScene: public RenderApp {
    private:
        // Godot's editor-preview sky; the tone map gives back its bytes
        // (98, 116, 140), 0.4 of a step clear of every rounding edge
        static constexpr Color SkyRadiance{0.139f, 0.212f, 0.356f, 1.0f};

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
        // a service of the World below
        PrimitiveActors primitiveActors;
        // the map's primitives as actors; the RenderScene rows are its
        World world;
        // outside the World: the Scene View looks through it and edits with it
        EditorCamera sceneCamera{MakeSceneCut()};
        std::optional<WorldEditor> editor;
        std::optional<SceneImage> sceneImage;
        // a hold the mouse took, which only the mouse drags and lets go
        bool mouseHolds = false;

        RAII<UIRenderer> uiRenderer;
        UIContext uiContext;
        // before the panel, whose rotation fields point at it
        EulerInspector eulerInspector;
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
        // where the Scene View's camera starts, with the depth range
        // DescribePipeline read
        static EditorCut MakeSceneCut();

        // after the scene file loads, before its rows land; `shading` is what
        // the toggle starts on
        virtual void OnRestyleMaterials(
            RenderScene&,
            const PlaygroundMaterials&,
            PlaygroundShading&
        ) {}

        void OnBuildGeometry(GeometryPool&) override {}
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
