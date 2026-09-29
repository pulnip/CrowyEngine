#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <type_traits>

#include "PackedTable.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
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

        u32 albedoMapID = 0;
        u32 normalMapID = 0;
        u32 mrMapID = 0;
        // what the translucent pass blends with; the opaque one writes 1
        f32 opacity = 1.0f;

        // two lanes the engine never reads; each shading model documents
        // its reading
        Vec4 custom0{};
        Vec4 custom1{};
    };
    static_assert(sizeof(MaterialData) == 80);
    static_assert(offsetof(MaterialData, emissive) == 16);
    static_assert(offsetof(MaterialData, albedoMapID) == 32);
    static_assert(offsetof(MaterialData, opacity) == 44);
    static_assert(offsetof(MaterialData, custom0) == 48);
    static_assert(offsetof(MaterialData, custom1) == 64);
    static_assert(std::is_trivially_copyable_v<MaterialData>);

    // which passes draw a material; a DrawFilter admits a mask of these
    enum class MaterialDomain : u32 {
        Opaque = 1u << 0,
        Translucent = 1u << 1,
    };

    // The slice of a pipeline state a material owns. Depth state is the pass's.
    struct MaterialPipelineDesc {
        RHIShaderDesc vertexShader{.entryPoint = "vs_main"};
        // the color passes' default; a pass may replace it
        RHIShaderDesc fragmentShader{.entryPoint = "fs_main"};
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
    };
}
