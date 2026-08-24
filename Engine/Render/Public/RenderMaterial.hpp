#pragma once

#include <cstddef>
#include <optional>
#include <type_traits>

#include "PackedTable.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

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
        u32 _pad0 = 0;
    };
    static_assert(sizeof(MaterialData) == 48);
    static_assert(offsetof(MaterialData, emissive) == 16);
    static_assert(offsetof(MaterialData, albedoMapID) == 32);
    static_assert(std::is_trivially_copyable_v<MaterialData>);

    // The slice of a pipeline state a material owns.
    struct MaterialPipelineDesc {
        RHIShaderDesc vertexShader{.entryPoint = "vs_main"};
        RHIShaderDesc fragmentShader{.entryPoint = "fs_main"};
        RHIPrimitiveTopology topology = RHIPrimitiveTopology::TriangleList;
        RHIRasterizerState rasterizer{};
        std::optional<RHIBlendState> blend = std::nullopt;
        RHIComparisonFunc depthFunc = RHIComparisonFunc::Less;
        bool depthWrite = true;
        CStr profile = "sm_6_8";
    };

    struct MaterialResource {
        MaterialData data{};
        MaterialPipelineDesc pipeline{};
    };
}
