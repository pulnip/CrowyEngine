#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numbers>
#include <set>

#include <gtest/gtest.h>

#include "BacklotImport.hpp"
#include "CameraComponent.hpp"
#include "EnvironmentComponent.hpp"
#include "ExpectNear.hpp"
#include "FileUtil.hpp"
#include "JsonLoader.hpp"
#include "LightComponent.hpp"
#include "MaterialAsset.hpp"
#include "MeshComponent.hpp"
#include "MeshGenerator.hpp"
#include "MiniStage.hpp"
#include "RenderScene.hpp"
#include "Scene.hpp"
#include "StageScene.hpp"
#include "System.hpp"
#include "World.hpp"

using namespace Crowy;

namespace
{
    constexpr StrView Mount = "Backlot/";

    // the import loaded back into a World, as an editor would open it
    class Imported {
    public:
        BacklotImport import;

    private:
        SystemRegistry noSystems;
        RenderScene scene;
        World world{noSystems};

    public:
        explicit Imported(const LoadedStage& stage)
            : import(importBacklot(stage)) {
            world.Provide(scene);
            loadScene(
                world,
                parseJsonString(import.texts.at(BacklotSceneFile))
            );
            world.Frame(0.0);
        }

        World& GetWorld() { return world; }

        ActorId Actor(StrView name) {
            auto found = ActorId::InvalidHandle();
            world.EachActor([&](ActorId actor, ActorId) {
                if(world.Name(actor) == name)
                    found = actor;
            });
            EXPECT_TRUE(found.IsValid()) << name;

            return found;
        }

        template<typename T>
        const T& Component(StrView name) {
            const auto* component = world.Get<T>(Actor(name));
            EXPECT_NE(component, nullptr) << name;

            return *component;
        }

        // the material file a ref names
        MaterialDesc Material(const AssetRef& ref) const {
            auto path = ref.ToString();
            EXPECT_TRUE(path.starts_with(Mount)) << path;

            return parseMaterialDesc(parseJsonString(import.texts.at(
                                         path.substr(Mount.size())
                                     )))
                .value();
        }
    };

    Str refOf(StrView path) {
        return std::format("{}{}", Mount, path);
    }

    StageQuad& quadNamed(LoadedStage& stage, StrView name) {
        return *std::ranges::find(stage.document.quads, name, &StageQuad::name);
    }

    StageInstance& instanceNamed(LoadedStage& stage, StrView name) {
        return *std::ranges::find(
            stage.document.instances,
            name,
            &StageInstance::name
        );
    }

    StageLightingKey& dayOf(LoadedStage& stage) {
        return stage.document.lightingKeys.front();
    }
}

TEST(BacklotImport, AQuadsFrontTopLeftTakesItsFirstUVWhereStageDrawsIt) {
    auto stage = miniStage();
    quadNamed(stage, "sign-a").yaw = 200.0f;
    const auto& quad = stage.document.quads[0];
    Imported imported(stage);

    const auto toWorld =
        imported.GetWorld().WorldMatrix(imported.Actor("sign-a"));
    const auto built = MakePlane(-unitZ(), unitX(), 0.5f);
    ASSERT_EQ(built.vertices.size(), stage.unitQuad.vertices.size());
    for(usize i = 0; i < built.vertices.size(); ++i) {
        const auto& ours = built.vertices[i];
        const auto& stages = stage.unitQuad.vertices[i];
        ASSERT_EQ(ours.texCoord, stages.texCoord);

        expectNear(
            transformPoint(toWorld, ours.position),
            transformPoint(quadToWorld(quad), stages.position)
        );
        expectNear(
            normalize(transformDirection(toWorld, ours.normal)),
            normalize(transformDirection(quadToWorld(quad), stages.normal))
        );
    }

    // the contract: the front faces (sin yaw, 0, cos yaw), the right edge lies
    // along (-cos yaw, 0, sin yaw), and (u0, v0) is the front's top-left
    const auto yaw = quad.yaw * std::numbers::pi_v<f32> / 180.0f;
    const Vec3 right{-std::cos(yaw), 0.0f, std::sin(yaw)};
    const auto topLeft =
        std::ranges::find(built.vertices, Vec2{0.0f, 0.0f}, &Vertex::texCoord);
    expectNear(
        transformPoint(toWorld, topLeft->position),
        quad.position - right * (0.5f * quad.width) +
            unitY() * (0.5f * quad.height)
    );
}

TEST(BacklotImport, EveryInstanceStandsWhereInstanceToWorldPutsIt) {
    auto stage = miniStage();
    instanceNamed(stage, "tower").yaw = 35.0f;
    instanceNamed(stage, "lamp-a").yaw = -120.0f;
    Imported imported(stage);

    ASSERT_EQ(stage.document.instances.size(), 2u);
    for(const auto& instance: stage.document.instances) {
        SCOPED_TRACE(instance.name);
        const auto toWorld =
            imported.GetWorld().WorldMatrix(imported.Actor(instance.name));
        const auto expected = instanceToWorld(instance);
        for(const Vec3 corner:
            {Vec3{0.5f, 0.0f, 0.5f},
             Vec3{-0.5f, 1.0f, 0.5f},
             Vec3{0.5f, 0.2f, -0.5f}}) {
            expectNear(
                transformPoint(toWorld, corner),
                transformPoint(expected, corner)
            );
        }
    }
}

TEST(BacklotImport, ARowNotShownInTheDefaultKeyIsImportedHidden) {
    auto stage = miniStage();
    quadNamed(stage, "sign-a").keys = {"night"};
    Imported imported(stage);

    EXPECT_FALSE(imported.Component<MeshComponent>("tower").visible);
    EXPECT_FALSE(imported.Component<MeshComponent>("sign-a").visible);
    EXPECT_TRUE(imported.Component<MeshComponent>("lamp-a").visible);
    EXPECT_EQ(imported.import.texts.count(BacklotSceneFile), 1u);

    quadNamed(stage, "sign-a").keys = {"night", "day"};
    Imported shown(stage);
    EXPECT_TRUE(shown.Component<MeshComponent>("sign-a").visible);
}

TEST(
    BacklotImport,
    ALightOffInTheKeyKeepsItsIntensityAndOnesScaleMultipliesIt
) {
    auto stage = miniStage();
    {
        Imported imported(stage);
        const auto& light = imported.Component<LightComponent>("lamp-a-light");
        EXPECT_FALSE(light.enabled);
        EXPECT_FLOAT_EQ(light.intensity, 60.0f);
        EXPECT_EQ(light.kind, LightKind::Spot);
        EXPECT_FALSE(light.castShadow);
        EXPECT_FLOAT_EQ(light.innerConeAngle, radiansOf(40.0f));
        EXPECT_FLOAT_EQ(light.outerConeAngle, radiansOf(60.0f));
    }

    dayOf(stage).lightGroups.front().scale = 1.5f;
    Imported lit(stage);
    const auto& light = lit.Component<LightComponent>("lamp-a-light");
    EXPECT_TRUE(light.enabled);
    EXPECT_FLOAT_EQ(light.intensity, 90.0f);
    // the spot shines down along the actor's +Z
    expectNear(
        forward(lit.GetWorld().WorldRotation(lit.Actor("lamp-a-light"))),
        -unitY()
    );
}

TEST(BacklotImport, AnEmissiveSlotNamesItsInstancesChannelMaterial) {
    auto stage = miniStage();
    auto neonLamp = instanceNamed(stage, "lamp-a");
    neonLamp.name = "lamp-b";
    neonLamp.emissiveChannel = "neon";
    stage.document.instances.push_back(neonLamp);
    Imported imported(stage);

    const auto palette = refOf("Materials/Palette.json");
    const auto lampA = imported.Component<MeshComponent>("lamp-a").materials;
    ASSERT_EQ(lampA.size(), 2u);
    EXPECT_EQ(lampA[0].ToString(), palette);
    EXPECT_EQ(
        lampA[1].ToString(),
        refOf("Materials/PaletteEmissive_fixtures.json")
    );
    const auto lampB = imported.Component<MeshComponent>("lamp-b").materials;
    ASSERT_EQ(lampB.size(), 2u);
    EXPECT_EQ(lampB[0].ToString(), palette);
    EXPECT_EQ(
        lampB[1].ToString(),
        refOf("Materials/PaletteEmissive_neon.json")
    );
    ASSERT_EQ(imported.Component<MeshComponent>("tower").materials.size(), 1u);
    EXPECT_EQ(
        imported.Component<MeshComponent>("tower").materials[0].ToString(),
        palette
    );

    // the day key: emissive_scale 2 times the channel's scale
    EXPECT_EQ(imported.Material(lampB[1]).data.emissive, 10.0f * ones());
    EXPECT_EQ(imported.Material(lampA[1]).data.emissive, 2.0f * ones());
    EXPECT_FALSE(imported.Material(lampB[1]).emissiveMap.texture.IsNone());
    EXPECT_TRUE(imported.Material(lampA[0]).emissiveMap.texture.IsNone());
    EXPECT_EQ(imported.Material(lampA[0]).data.emissive, zeros());
}

TEST(BacklotImport, AMaterialFileCarriesTheRowsTextureSamplerAndCut) {
    auto stage = miniStage();
    stage.document.materials[2].kind = StageMaterialKind::Masked;
    stage.document.materials[2].cutoff = 0.4f;
    Imported imported(stage);

    const auto sign = imported.Material(
        imported.Component<MeshComponent>("sign-a").materials[0]
    );
    EXPECT_EQ(sign.domain, MaterialDomain::Masked);
    EXPECT_FLOAT_EQ(sign.data.alphaCutoff, 0.4f);
    EXPECT_FALSE(sign.receiveShadow);
    EXPECT_FLOAT_EQ(sign.data.roughness, StageRoughness);
    EXPECT_EQ(sign.albedoMap.texture.ToString(), refOf("Signs.png"));
    EXPECT_EQ(sign.albedoMap.sampler, TextureSampler::LinearClamp);
    EXPECT_EQ(sign.data.uvScaleOffset, (Vec4{0.25f, 0.25f, 0.25f, 0.5f}));
    // the signs channel is dark by day, but the quad still has its map
    EXPECT_EQ(sign.data.emissive, zeros());
    EXPECT_EQ(sign.emissiveMap.texture.ToString(), refOf("Signs.png"));

    const auto palette = imported.Material(
        imported.Component<MeshComponent>("tower").materials[0]
    );
    EXPECT_EQ(palette.domain, MaterialDomain::Opaque);
    EXPECT_EQ(palette.albedoMap.sampler, TextureSampler::NearestClamp);
    EXPECT_TRUE(palette.receiveShadow);
}

TEST(BacklotImport, QuadsShareAMaterialFileOnlyForOneMaterialImageAndChannel) {
    auto stage = miniStage();
    auto twin = quadNamed(stage, "sign-a");
    twin.name = "sign-b";
    twin.position.x += 3.0f;
    auto lit = twin;
    lit.name = "sign-c";
    lit.emissiveChannel.clear();
    stage.document.quads.push_back(twin);
    stage.document.quads.push_back(lit);
    Imported imported(stage);

    std::set<Str> quadFiles;
    for(const auto& [path, text]: imported.import.texts) {
        if(path.starts_with("Materials/Signs_"))
            quadFiles.insert(path);
    }
    EXPECT_EQ(
        quadFiles,
        (std::set<Str>{
            "Materials/Signs_a.json",
            "Materials/Signs_a_signs.json"
        })
    );
    EXPECT_EQ(
        imported.Component<MeshComponent>("sign-b").materials,
        imported.Component<MeshComponent>("sign-a").materials
    );
    EXPECT_NE(
        imported.Component<MeshComponent>("sign-c").materials,
        imported.Component<MeshComponent>("sign-a").materials
    );

    quadNamed(stage, "sign-b").uv1.x += 0.1f;
    EXPECT_THROW(importBacklot(stage), std::runtime_error);
}

TEST(BacklotImport, TheEnvironmentActorHoldsTheDefaultKeysAmbientSkyAndSun) {
    auto stage = miniStage();
    auto& day = dayOf(stage);
    day.sunDirection = normalize(Vec3{0.3531f, -0.788f, 0.5043f});
    day.sunShadows = false;
    day.ambientSky = {0.2f, 0.4f, 0.8f};
    day.skyHorizon = {0.1f, 0.3f, 0.5f};
    Imported imported(stage);

    const auto& environment =
        imported.Component<EnvironmentComponent>("environment");
    EXPECT_EQ(environment.skyAmbient, (Vec3{0.2f, 0.4f, 0.8f}) * 0.5f);
    EXPECT_EQ(environment.groundAmbient, zeros());
    EXPECT_EQ(environment.skyColor, (Vec3{0.1f, 0.3f, 0.5f}));

    const auto& sun = imported.Component<LightComponent>("environment");
    EXPECT_EQ(sun.kind, LightKind::Directional);
    EXPECT_FALSE(sun.castShadow);
    EXPECT_TRUE(sun.enabled);
    EXPECT_FLOAT_EQ(sun.intensity, 3.0f);
    expectNear(
        forward(
            imported.GetWorld().WorldRotation(imported.Actor("environment"))
        ),
        day.sunDirection
    );
}

TEST(BacklotImport, TheFirstCutIsTheLowestFileIdAndLooksWhereTheCutDoes) {
    auto stage = miniStage();
    Imported imported(stage);
    const auto cuts = makeStageCuts(stage);
    auto& world = imported.GetWorld();

    EXPECT_LT(
        world.FileId(imported.Actor("street")),
        world.FileId(imported.Actor("close"))
    );
    EXPECT_LT(
        world.FileId(imported.Actor("close")),
        world.FileId(imported.Actor("between"))
    );
    for(const auto& cut: cuts) {
        SCOPED_TRACE(cut.name);
        const auto actor = imported.Actor(cut.name);
        const auto& camera = imported.Component<CameraComponent>(cut.name);
        EXPECT_FLOAT_EQ(camera.fovY, cut.fovY);
        EXPECT_FLOAT_EQ(camera.nearZ, cut.nearZ);
        EXPECT_FLOAT_EQ(camera.farZ, cut.farZ);
        expectNear(world.WorldPosition(actor), cut.position);
        // pitch is down-positive in a cut
        expectNear(
            forward(world.WorldRotation(actor)),
            Vec3{
                std::sin(cut.yaw) * std::cos(cut.pitch),
                -std::sin(cut.pitch),
                std::cos(cut.yaw) * std::cos(cut.pitch)
            }
        );
    }
    // the street cut looks up by four degrees
    EXPECT_GT(forward(world.WorldRotation(imported.Actor("street"))).y, 0.06f);
}

TEST(BacklotImport, OnlyTheFilesTheSceneNamesAreCopiedWithTheSourceScene) {
    auto stage = miniStage();
    const auto imported = importBacklot(stage);

    std::set<Str> from;
    std::set<Str> to;
    for(const auto& copy: imported.copies) {
        from.insert(copy.from);
        to.insert(copy.to);
    }
    EXPECT_EQ(
        to,
        (std::set<Str>{
            "Models/Box.fbx",
            "Models/Lamp.fbx",
            "Palette.png",
            "Signs.png",
            "Source/scene.json"
        })
    );
    EXPECT_TRUE(from.contains("Data/scene.json"));
    EXPECT_EQ(imported.copies.size(), 5u);

    stage.document.materials.pop_back();
    stage.document.quads.clear();
    const auto without = importBacklot(stage);
    EXPECT_EQ(without.copies.size(), 4u);
}

TEST(BacklotImport, TheFilesRoundTripByteForByteAndRunsAgreeWithEachOther) {
    auto stage = miniStage();
    Imported imported(stage);

    const auto path =
        std::filesystem::temp_directory_path() / "BacklotImportRoundTrip.json";
    saveSceneFile(imported.GetWorld(), path);
    EXPECT_EQ(
        readFileAsString(path),
        imported.import.texts.at(BacklotSceneFile)
    );
    std::filesystem::remove(path);

    for(const auto& [name, text]: imported.import.texts) {
        if(name == BacklotSceneFile)
            continue;

        SCOPED_TRACE(name);
        EXPECT_EQ(
            emitJson(
                serializeMaterialDesc(
                    parseMaterialDesc(parseJsonString(text)).value()
                ),
                JsonStyle::Pretty
            ) + "\n",
            text
        );
    }

    const auto again = importBacklot(stage);
    EXPECT_EQ(again.texts, imported.import.texts);
    EXPECT_EQ(again.report, imported.import.report);
}

TEST(BacklotImport, TheReportCountsWhatItLeftOut) {
    auto stage = miniStage();
    stage.document.cameras[1].orthographic = true;
    stage.document.cameras[1].orthoSize = 52.0f;
    const auto report = importBacklot(stage).report;

    EXPECT_NE(
        report.find("hidden in 'day' (visible:false): 1 instances, 0 quads"),
        Str::npos
    ) << report;
    EXPECT_NE(report.find("lights off in 'day', intensity kept: 1"), Str::npos)
        << report;
    EXPECT_NE(report.find("lighting keys: night"), Str::npos) << report;
    EXPECT_NE(
        report.find("orthographic: 'close' (half height 52 m)"),
        Str::npos
    ) << report;
}
