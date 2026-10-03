#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <type_traits>

#include "PackedTable.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "RenderTexture.hpp"
#include "ShadingModel.hpp"

namespace Crowy
{
    struct MaterialResource;

    using MaterialHandle = GenericHandle<MaterialResource>;
    using MaterialTable = PackedTable<MaterialResource>;

    // glTF 2.0 metallic-roughness.
    struct MaterialData {
        Vec3 albedo = ones();
        f32 metallic = 0.0f;
        Vec3 emissive = zeros();
        f32 roughness = 0.5f;

        // 1 + a row of the frame's texture table, 0 for none; the renderer
        // writes the albedo and emissive ones from MaterialResource::maps
        u32 albedoMapID = 0;
        u32 normalMapID = 0;
        u32 mrMapID = 0;
        // what the translucent pass blends with, times the albedo texel's
        // alpha; the opaque one writes 1
        f32 opacity = 1.0f;

        // two lanes the engine never reads; each shading model documents
        // its reading
        Vec4 custom0{};
        Vec4 custom1{};

        // uv' = uv * xy + zw before any map is read
        Vec4 uvScaleOffset{1.0f, 1.0f, 0.0f, 0.0f};
        // the emission is `emissive` times this map's texel
        u32 emissiveMapID = 0;
        // a Masked material cuts where its opacity falls below it
        f32 alphaCutoff = 0.5f;
        // MaterialFlags
        u32 flags = 0;
        u32 _pad = 0;
    };
    static_assert(sizeof(MaterialData) == 112);
    static_assert(offsetof(MaterialData, emissive) == 16);
    static_assert(offsetof(MaterialData, albedoMapID) == 32);
    static_assert(offsetof(MaterialData, opacity) == 44);
    static_assert(offsetof(MaterialData, custom0) == 48);
    static_assert(offsetof(MaterialData, custom1) == 64);
    static_assert(offsetof(MaterialData, uvScaleOffset) == 80);
    static_assert(offsetof(MaterialData, emissiveMapID) == 96);
    static_assert(offsetof(MaterialData, alphaCutoff) == 100);
    static_assert(offsetof(MaterialData, flags) == 104);
    static_assert(std::is_trivially_copyable_v<MaterialData>);

    // MaterialData::flags, mirrored in Engine/Shader/SceneData.slang
    enum class MaterialFlags : u32 {
        None = 0,
        // lights reach the surface unshadowed, as on a painted backdrop
        NoShadowReceive = 1u << 0,
    };

    // the textures a material samples; the renderer turns them into the
    // row's map IDs each frame, so a removed texture reads as none
    struct MaterialMaps {
        TextureHandle albedo;
        TextureHandle emissive;
    };

    // which passes draw a material; a DrawFilter admits a mask of these
    enum class MaterialDomain : u32 {
        Opaque = 1u << 0,
        Translucent = 1u << 1,
        // opaque where opacity reaches alphaCutoff, cut elsewhere; in depth
        // too, where the depth-only passes run its maskShader
        Masked = 1u << 2,
    };

    // The slice of a pipeline state a material owns. Depth state is the pass's.
    struct MaterialPipelineDesc {
        RHIShaderDesc vertexShader{.entryPoint = "vs_main"};
        // the color passes' default; a pass may replace it
        RHIShaderDesc fragmentShader{.entryPoint = "fs_main"};
        // a Masked material's entry in the depth-only passes: the same cut,
        // no color
        RHIShaderDesc maskShader{.entryPoint = "fs_masked_depth"};
        // linked into the colour passes' programs; empty links nothing
        std::filesystem::path shadingModule = PBRShadingModule;
        RHIPrimitiveTopology topology = RHIPrimitiveTopology::TriangleList;
        // every pass culls alike, or Equal fails on double-sided back faces
        RHIRasterizerState rasterizer{};
        std::optional<RHIBlendState> blend = std::nullopt;
        MaterialDomain domain = MaterialDomain::Opaque;
        CStr profile = "sm_6_8";
    };

    struct MaterialResource {
        MaterialData data{};
        MaterialPipelineDesc pipeline{};
        MaterialMaps maps{};
    };
}
