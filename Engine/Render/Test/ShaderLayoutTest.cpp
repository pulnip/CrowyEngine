#include <array>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>
#include <slang-com-ptr.h>
#include <slang.h>

#include "PathUtil.hpp"
#include "ProjectRoot.hpp"
#include "RenderLight.hpp"
#include "RenderMaterial.hpp"
#include "RenderSceneData.hpp"
#include "RenderTexture.hpp"
#include "Vertex.hpp"

#define CROWY_FIELD(TYPE, MEMBER)                                              \
    FieldLayout {                                                              \
        #MEMBER, offsetof(TYPE, MEMBER), sizeof(TYPE::MEMBER)                  \
    }

using namespace Crowy;

namespace
{
    struct FieldLayout {
        StrView name;
        usize offset = 0;
        usize size = 0;
    };

    using FieldLayouts = std::vector<FieldLayout>;

    // the engine's shader library as both backends lay it out
    class SlangLayouts {
    public:
        static constexpr std::array Targets{SLANG_HLSL, SLANG_METAL};

    private:
        Slang::ComPtr<slang::IGlobalSession> global;
        Slang::ComPtr<slang::ISession> session;

    public:
        SlangLayouts() {
            const SlangGlobalSessionDesc globalDesc{};
            if(SLANG_FAILED(
                   slang_createGlobalSession2(&globalDesc, global.writeRef())
               ))
                throw std::runtime_error("no Slang global session");

            std::array<slang::TargetDesc, Targets.size()> targets{};
            for(usize i = 0; i < Targets.size(); ++i)
                targets[i].format = Targets[i];
            const auto library =
                toUTF8String(resolveProjectPath("Engine/Shader"));
            const char* searchPaths[] = {library.c_str()};
            const slang::SessionDesc sessionDesc{
                .targets = targets.data(),
                .targetCount = static_cast<SlangInt>(targets.size()),
                .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
                .searchPaths = searchPaths,
                .searchPathCount = 1
            };
            if(SLANG_FAILED(
                   global->createSession(sessionDesc, session.writeRef())
               ))
                throw std::runtime_error("no Slang session");
        }

        slang::ProgramLayout& Load(StrView file, usize target) {
            const auto path =
                toUTF8String(resolveProjectPath("Engine/Shader") / file);
            auto* module = session->loadModule(path.c_str());
            if(module == nullptr)
                throw std::runtime_error("a shader failed to load");

            return *module->getLayout(static_cast<SlangInt>(target));
        }
    };

    FieldLayouts fieldsOf(slang::TypeLayoutReflection& type) {
        FieldLayouts fields;
        for(u32 i = 0; i < type.getFieldCount(); ++i) {
            auto& field = *type.getFieldByIndex(i);
            const StrView name = field.getName();
            if(name.starts_with("_pad"))
                continue;
            fields.push_back(
                {name, field.getOffset(), field.getTypeLayout()->getSize()}
            );
        }

        return fields;
    }

    FieldLayouts fieldsOf(
        slang::ProgramLayout& layout,
        CStr type,
        slang::LayoutRules rules
    ) {
        auto* found = layout.findTypeByName(type);
        if(found == nullptr)
            return {};

        return fieldsOf(*layout.getTypeLayout(found, rules));
    }

    // Slang's Metal reflection gives a DescriptorHandle its 8 bytes only under
    // the tier-2 argument buffer rules
    slang::LayoutRules handleRules(usize target, slang::LayoutRules rules) {
        return SlangLayouts::Targets[target] == SLANG_METAL
                   ? slang::LayoutRules::MetalArgumentBufferTier2
                   : rules;
    }

    // a cbuffer block's members
    FieldLayouts blockFieldsOf(slang::ProgramLayout& layout, StrView block) {
        for(u32 i = 0; i < layout.getParameterCount(); ++i) {
            auto& parameter = *layout.getParameterByIndex(i);
            if(StrView{parameter.getName()} == block)
                return fieldsOf(
                    *parameter.getTypeLayout()->getElementTypeLayout()
                );
        }

        return {};
    }

    void expectFields(
        const FieldLayouts& shader,
        const FieldLayouts& cpp,
        usize target
    ) {
        ASSERT_EQ(shader.size(), cpp.size()) << "target " << target;
        for(usize i = 0; i < cpp.size(); ++i) {
            EXPECT_EQ(shader[i].name, cpp[i].name) << "target " << target;
            EXPECT_EQ(shader[i].offset, cpp[i].offset)
                << cpp[i].name << ", target " << target;
            EXPECT_EQ(shader[i].size, cpp[i].size)
                << cpp[i].name << ", target " << target;
        }
    }
}

// a field reordered or retyped on one side compiles on both and only shows
// as a wrong picture
TEST(ShaderLayout, TheSceneStructsMatchTheirSlangTwins) {
    using enum slang::LayoutRules;

    const FieldLayouts vertex{
        CROWY_FIELD(Vertex, position),
        CROWY_FIELD(Vertex, normal),
        CROWY_FIELD(Vertex, texCoord),
        CROWY_FIELD(Vertex, tangent)
    };
    const FieldLayouts draw{
        CROWY_FIELD(DrawData, world),
        CROWY_FIELD(DrawData, materialIndex),
        CROWY_FIELD(DrawData, objectID),
        CROWY_FIELD(DrawData, vbIndex)
    };
    const FieldLayouts material{
        CROWY_FIELD(MaterialData, albedo),
        CROWY_FIELD(MaterialData, metallic),
        CROWY_FIELD(MaterialData, emissive),
        CROWY_FIELD(MaterialData, roughness),
        CROWY_FIELD(MaterialData, albedoMapID),
        CROWY_FIELD(MaterialData, normalMapID),
        CROWY_FIELD(MaterialData, mrMapID),
        CROWY_FIELD(MaterialData, opacity),
        CROWY_FIELD(MaterialData, custom0),
        CROWY_FIELD(MaterialData, custom1),
        CROWY_FIELD(MaterialData, uvScaleOffset),
        CROWY_FIELD(MaterialData, emissiveMapID),
        CROWY_FIELD(MaterialData, alphaCutoff),
        CROWY_FIELD(MaterialData, flags)
    };
    const FieldLayouts texture{
        CROWY_FIELD(TextureData, texture),
        CROWY_FIELD(TextureData, sampler)
    };
    const FieldLayouts light{
        CROWY_FIELD(LightData, worldToShadow),
        CROWY_FIELD(LightData, position),
        CROWY_FIELD(LightData, invRange),
        CROWY_FIELD(LightData, direction),
        CROWY_FIELD(LightData, kind),
        CROWY_FIELD(LightData, color),
        CROWY_FIELD(LightData, shadowIndex),
        CROWY_FIELD(LightData, coneScale),
        CROWY_FIELD(LightData, coneOffset),
        CROWY_FIELD(LightData, shadowBias),
        CROWY_FIELD(LightData, shadowNormalBias)
    };
    const FieldLayouts push{
        CROWY_FIELD(ScenePush, draws),
        CROWY_FIELD(ScenePush, materials),
        CROWY_FIELD(ScenePush, vertices),
        CROWY_FIELD(ScenePush, drawBase),
        CROWY_FIELD(ScenePush, materialBase),
        CROWY_FIELD(ScenePush, lights),
        CROWY_FIELD(ScenePush, lightBase),
        CROWY_FIELD(ScenePush, lightCount),
        CROWY_FIELD(ScenePush, shadowMap),
        CROWY_FIELD(ScenePush, textures),
        CROWY_FIELD(ScenePush, textureBase)
    };
    const FieldLayouts view{
        CROWY_FIELD(ViewData, viewProj),
        CROWY_FIELD(ViewData, debugMode),
        CROWY_FIELD(ViewData, shadowFilter),
        CROWY_FIELD(ViewData, cameraPosition),
        CROWY_FIELD(ViewData, skyAmbient),
        CROWY_FIELD(ViewData, groundAmbient)
    };
    const FieldLayouts fullscreen{
        CROWY_FIELD(FullscreenPush, source),
        CROWY_FIELD(FullscreenPush, input0),
        CROWY_FIELD(FullscreenPush, input1),
        CROWY_FIELD(FullscreenPush, input2),
        CROWY_FIELD(FullscreenPush, params)
    };

    SlangLayouts layouts;
    for(usize target = 0; target < SlangLayouts::Targets.size(); ++target) {
        auto& scene = layouts.Load("SceneData.slang", target);
        expectFields(
            fieldsOf(scene, "Vertex", DefaultStructuredBuffer),
            vertex,
            target
        );
        expectFields(
            fieldsOf(scene, "DrawData", DefaultStructuredBuffer),
            draw,
            target
        );
        expectFields(
            fieldsOf(scene, "MaterialData", DefaultStructuredBuffer),
            material,
            target
        );
        expectFields(
            fieldsOf(
                scene,
                "TextureData",
                handleRules(target, DefaultStructuredBuffer)
            ),
            texture,
            target
        );
        expectFields(
            fieldsOf(scene, "LightData", DefaultStructuredBuffer),
            light,
            target
        );
        expectFields(
            fieldsOf(
                scene,
                "ScenePush",
                handleRules(target, DefaultConstantBuffer)
            ),
            push,
            target
        );
        expectFields(blockFieldsOf(scene, "view"), view, target);

        auto& fullscreenLayout = layouts.Load("Fullscreen.slang", target);
        expectFields(
            fieldsOf(
                fullscreenLayout,
                "FullscreenPush",
                handleRules(target, DefaultConstantBuffer)
            ),
            fullscreen,
            target
        );
    }
}
