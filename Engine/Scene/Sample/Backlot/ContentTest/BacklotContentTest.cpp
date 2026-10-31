#include <algorithm>
#include <filesystem>
#include <set>

#include <gtest/gtest.h>

#include "DOM.hpp"
#include "JsonLoader.hpp"
#include "PathUtil.hpp"
#include "StringUtil.hpp"

using namespace Crowy;

namespace
{
    using Names = std::multiset<Str>;
    using Refs = std::set<Str>;

    constexpr CStr QuadMesh = "engine:Builtin/Quad";

    const auto contentRoot = toPath(CROWY_CONTENT_ROOT);
    const auto importRoot = contentRoot / "Backlot";

    // what the source's rows say the scene file holds
    struct SourceRows {
        Names instances;
        Names quads;
        Names lights;
        Names cameras;
        Refs models;
        usize hidden = 0;
    };

    // what the scene file holds, by the component that makes an actor one
    struct SceneRows {
        Names instances;
        Names quads;
        Names lights;
        Names cameras;
        Refs models;
        Refs materials;
        usize hidden = 0;
        usize suns = 0;
        usize environments = 0;
    };

    const DOM::Array& arrayAt(const DOM::Value& value, StrView key) {
        return *value.asTable()->find(Str(key))->second.asArray();
    }

    Str textAt(const DOM::Value& value, StrView key) {
        return value.get<Str>(key).value_or("");
    }

    bool showsIn(const DOM::Value& row, StrView key) {
        const auto keys = row.asTable()->find("keys");
        if(keys == row.asTable()->end())
            return true;

        return std::ranges::any_of(
            *keys->second.asArray(),
            [&](const DOM::Value& name) { return *name.asString() == key; }
        );
    }

    Names namesOf(const DOM::Array& rows) {
        Names names;
        for(const auto& row: rows)
            names.insert(textAt(row, "name"));

        return names;
    }

    SourceRows readSource() {
        const auto source = parseJsonFile(importRoot / "Source/scene.json");
        const auto defaultKey = textAt(source, "default_key");

        SourceRows rows{
            .instances = namesOf(arrayAt(source, "instances")),
            .quads = namesOf(arrayAt(source, "quads")),
            .lights = namesOf(arrayAt(source, "lights")),
            .cameras = namesOf(arrayAt(source, "cameras"))
        };
        for(const auto& instance: arrayAt(source, "instances")) {
            rows.models.insert(textAt(instance, "model"));
            rows.hidden += showsIn(instance, defaultKey) ? 0 : 1;
        }
        for(const auto& quad: arrayAt(source, "quads"))
            rows.hidden += showsIn(quad, defaultKey) ? 0 : 1;

        return rows;
    }

    void readMesh(
        const DOM::Value& component,
        const Str& name,
        SceneRows& rows
    ) {
        const auto mesh = textAt(component, "mesh");
        if(mesh == QuadMesh) {
            rows.quads.insert(name);
        }
        else {
            rows.instances.insert(name);
            rows.models.insert(mesh);
        }

        rows.hidden += component.get<bool>("visible").value_or(true) ? 0 : 1;
        for(const auto& material: arrayAt(component, "materials"))
            rows.materials.insert(*material.asString());
    }

    SceneRows readScene() {
        const auto scene = parseJsonFile(importRoot / "Backlot.json");

        SceneRows rows;
        for(const auto& actor: arrayAt(scene, "actors")) {
            const auto name = textAt(actor, "name");
            for(const auto& component: arrayAt(actor, "components")) {
                const auto type = textAt(component, "type");
                if(type == "MeshComponent") {
                    readMesh(component, name, rows);
                }
                else if(type == "CameraComponent") {
                    rows.cameras.insert(name);
                }
                else if(type == "EnvironmentComponent") {
                    ++rows.environments;
                }
                else if(textAt(component, "kind") == "Directional") {
                    ++rows.suns;
                }
                else {
                    rows.lights.insert(name);
                }
            }
        }

        return rows;
    }

    // a content ref, which every ref but a built-in is
    std::filesystem::path fileOf(const Str& ref) {
        return contentRoot / toPath(ref);
    }
}

TEST(BacklotContent, TheSceneFileHoldsEveryRowOfItsSource) {
    const auto source = readSource();
    const auto scene = readScene();

    EXPECT_EQ(scene.instances, source.instances);
    EXPECT_EQ(scene.quads, source.quads);
    EXPECT_EQ(scene.lights, source.lights);
    EXPECT_EQ(scene.cameras, source.cameras);
    EXPECT_EQ(scene.models.size(), source.models.size());
    EXPECT_EQ(scene.hidden, source.hidden);
    EXPECT_EQ(scene.suns, 1u);
    EXPECT_EQ(scene.environments, 1u);
}

TEST(BacklotContent, EveryRefTheSceneNamesIsAFile) {
    const auto scene = readScene();

    for(const auto& model: scene.models)
        EXPECT_TRUE(std::filesystem::is_regular_file(fileOf(model))) << model;

    Refs textures;
    for(const auto& ref: scene.materials) {
        ASSERT_TRUE(std::filesystem::is_regular_file(fileOf(ref))) << ref;

        const auto material = parseJsonFile(fileOf(ref));
        textures.insert(textAt(material, "albedoMap.texture"));
        if(const auto emissive = material.get<Str>("emissiveMap.texture"))
            textures.insert(*emissive);
    }

    for(const auto& texture: textures)
        EXPECT_TRUE(std::filesystem::is_regular_file(fileOf(texture)))
            << texture;
}
