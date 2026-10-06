// The painted surfaces, drawn in the paint hook pass with the engine's lights
// and shadow map: MF_PaintOverlay's atlas lookup, then the look or one of the
// data views the topics are shown with.

#include "Lighting.slang"
#include "PaintCommon.slang"
#include "Sampler.slang"
#include "SceneData.slang"

// mirrored from PaintSurfaceConstants in PaintLabGpu.hpp
cbuffer surface : register(b2) {
    // Mint-local cm to Crowy world metres
    float4x4 objectToWorld;
    // Mint-local to Mint-world, rotation only, by rows
    float4 rotation0;
    float4 rotation1;
    float4 rotation2;
    float4 scale3D;
    // the mesh's own bounds, unscaled, as the position atlas is normalized
    float4 boundsMin;
    float4 boundsSize;
    // PaintIsland_*: uv offset in xy, uv scale in zw, zero when off
    float4 islands[6];
    // x size, y 1 / size, z cm per texel, w distance range
    float4 atlas;
    // x paint, y position, z edge fade: rows of the frame's texture table;
    // w the pool row of the mesh's first vertex
    uint4 maps;
    // x the view left of the split, y right of it
    uint4 modes;
    // x the split's pixel column
    float4 screen;
};

struct SurfaceOut {
    float4 posProj : SV_Position;
    float3 posWorld : POSITION;
    // Mint-local cm and the mesh's own normal there
    float3 localPos : LOCALPOS;
    float3 localNormal : LOCALNORMAL;
};

[shader("vertex")]
SurfaceOut vs_surface(uint vertexID : SV_VertexID) {
    StructuredBuffer<Vertex> vertices = pass.vertices;
    Vertex input = vertices[maps.w + vertexID];
    float4 posWorld = mul(objectToWorld, float4(input.position, 1.0));

    SurfaceOut output;
    output.posProj = mul(viewProj, posWorld);
    output.posWorld = posWorld.xyz;
    output.localPos = input.position;
    output.localNormal = input.normal;

    return output;
}

Texture2D tableTexture(uint mapID) {
    return LoadTexture(mapID).texture;
}

// Mint-local to Crowy world for a normal: undo the scale the way an inverse
// transpose does, rotate, then the boundary's (y, z, x)
float3 normalToWorld(float3 localNormal) {
    float3 n = localNormal / scale3D.xyz;
    float3 mint = float3(dot(rotation0.xyz, n), dot(rotation1.xyz, n),
                         dot(rotation2.xyz, n));

    return normalize(float3(mint.y, mint.z, mint.x));
}

// the bare look under the paint, ML_Look_Plain, lit by every light
float3 shadePlain(float3 albedo, float roughness, float3 N, float3 posWorld) {
    float3 V = normalize(cameraPosition.xyz - posWorld);
    float a = max(roughness * roughness, 1e-3);
    float a2 = a * a;
    float3 color = float3(0.0, 0.0, 0.0);
    for(uint i = 0; i < LightCount(); ++i) {
        LightSample light = SampleLight(i, posWorld, N);
        float3 L = light.direction;
        float3 H = normalize(L + V);
        float NoL = saturate(dot(N, L));
        float NoV = max(dot(N, V), 1e-4);
        float NoH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));
        float dd = NoH * NoH * (a2 - 1.0) + 1.0;
        float D = a2 / (3.14159265 * dd * dd);
        float vis = 0.5 / (NoL * sqrt(NoV * NoV * (1.0 - a2) + a2) +
                           NoV * sqrt(NoL * NoL * (1.0 - a2) + a2) + 1e-5);
        float F = 0.04 + 0.96 * pow(1.0 - VoH, 5.0);
        float3 diffuse = (1.0 - F) * albedo / 3.14159265;
        color += (diffuse + D * vis * F) * light.radiance * light.attenuation * NoL;
    }

    return color + AmbientIrradiance(N) * albedo;
}

[shader("fragment")]
float4 fs_surface(SurfaceOut input) : SV_Target {
    float3 geoNormal = normalToWorld(input.localNormal);
    // MF_PaintOverlay: the island the normal leans toward, then the atlas UV
    float3 n = (input.localPos - boundsMin.xyz) / max(boundsSize.xyz, 1e-4);
    uint direction = IslandDirection(input.localNormal / scale3D.xyz);
    float4 island = islands[direction];
    float2 uv = IslandUV(n, island, direction, atlas.y);
    int3 texel = int3(int2(floor(uv * atlas.x)), 0);
    int last = int(atlas.x) - 1;
    texel.xy = clamp(texel.xy, int2(0, 0), int2(last, last));

    uint view = input.posProj.x < screen.x ? modes.x : modes.y;
    float4 paint = tableTexture(maps.x).Load(texel);
    uint id = DecodePaintId(paint.r);

    float3 display;
    switch(view) {
    case PAINT_VIEW_ISLANDS: {
        if(island.z <= 0.0) {
            // a direction this surface keeps no paint on
            float stripe = frac((input.posProj.x + input.posProj.y) / 24.0);
            display = stripe < 0.5 ? float3(0.24, 0.24, 0.26) : float3(0.32, 0.32, 0.34);
            break;
        }
        float2 block = floor(uv * atlas.x / 16.0);
        float checker = fmod(block.x + block.y, 2.0);
        display = DirectionColor(direction) * (checker > 0.5 ? 0.82 : 1.0);
        break;
    }
    case PAINT_VIEW_PAINT_ID:
        display = TeamDisplayColor(id);
        break;
    case PAINT_VIEW_HEIGHT:
        display = RampColor(paint.g);
        break;
    case PAINT_VIEW_DISTANCE:
        display = RampColor(paint.b);
        break;
    case PAINT_VIEW_POSITION:
        display = saturate(tableTexture(maps.y).Load(texel).xyz);
        break;
    case PAINT_VIEW_EDGE_FADE:
        display = RampColor(tableTexture(maps.z).SampleLevel(linearClamp, uv, 0.0).r);
        break;
    default: {
        float3 albedo = id < 4u
            ? ((id & 1u) == 0u ? float3(0.0, 0.8, 0.505794) : float3(0.158, 0.04661, 0.01738))
            : float3(0.72, 0.70, 0.66);
        return float4(shadePlain(albedo, 0.6, geoNormal, input.posWorld), 1.0);
    }
    }

    return float4(DisplayToRadiance(display), 1.0);
}
