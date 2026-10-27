#include "Noise.slang"
#include "Sampler.slang"
#include "SRGB.slang"

// Triplanar: the isosurface mesh has no UVs, and a cave wall or an overhang
// has no sensible top-down projection.

// Mirrors TerrainVertex in Terrain.hpp.
struct TerrainVertex {
    float3 position;
    float3 normal;
};

struct ResourceData {
    // Vertex is written by GPU, so use structured buffer instead input layout
    DescriptorHandle<StructuredBuffer<TerrainVertex>> vertices;
};

[[vk::push_constant]]
ConstantBuffer<ResourceData> resource : register(b0);

cbuffer frame : register(b1) {
    float4x4 viewProj;
    float4 toLight_ambient;
};

struct VertexOut {
    float4 posProj : SV_Position;
    float3 posWorld : TEXCOORD0;
    float3 normal : NORMAL;
};

[shader("vertex")]
VertexOut vs_main(uint vertexID : SV_VertexID) {
    StructuredBuffer<TerrainVertex> vertices = resource.vertices;
    TerrainVertex vertex = vertices[vertexID];

    VertexOut output;
    output.posProj = mul(viewProj, float4(vertex.position, 1.0));
    output.posWorld = vertex.position;
    output.normal = vertex.normal;

    return output;
}

// Content/Assets has no terrain textures, so the materials are two-tone
// gradients driven by the density field's fBm, in the same visual family.
static const uint SURFACE_SEED = 0x9d7fbe21u;

// in cycles per world unit; the grid is 128 wide, so this is a few metres
// of mottling rather than a pattern you can count
static const float DETAIL_FREQ = 0.35f;
static const uint DETAIL_OCTAVES = 3;

// how sharply the triplanar blend favours the dominant axis. Low values
// smear all three planes together, high ones bring back visible seams
static const float BLEND_SHARPNESS = 4.0f;

// Linear reflectances, not display colors: the shading runs in linear and
// encodes once at the end.
static const float3 GRASS_LOW = float3(0.020, 0.055, 0.014);
static const float3 GRASS_HIGH = float3(0.075, 0.170, 0.030);
static const float3 ROCK_LOW = float3(0.035, 0.032, 0.030);
static const float3 ROCK_HIGH = float3(0.230, 0.215, 0.190);

static const float ROCK_START = 0.78f;
static const float ROCK_END = 0.46f;

// A flat ambient leaves every wall and cave ceiling the sun misses black;
// these spread it over a sky/ground hemisphere as gains, not colors.
static const float3 SKY_FILL = float3(0.95, 1.10, 1.40);
static const float3 GROUND_BOUNCE = float3(1.20, 1.02, 0.82);

float detailAt(float2 uv) {
    float n = fbm(
        uv.x * DETAIL_FREQ,
        uv.y * DETAIL_FREQ,
        SURFACE_SEED,
        DETAIL_OCTAVES
    );

    return saturate(0.5f + 0.5f * n);
}

float3 planeColor(float2 uv, float grass) {
    float detail = detailAt(uv);

    float3 rock = lerp(ROCK_LOW, ROCK_HIGH, detail);
    float3 grassColor = lerp(GRASS_LOW, GRASS_HIGH, detail);

    return lerp(rock, grassColor, grass);
}

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    float3 N = normalize(input.normal);
    float3 P = input.posWorld;

    // a plain smoothstep on N.y draws the grass line as a clean contour that
    // reads as a decal; low-frequency noise on the threshold breaks it up
    float wobble = fbm(P.x * 0.05f, P.z * 0.05f, SURFACE_SEED ^ 0x51u, 2);
    float grass = smoothstep(ROCK_END, ROCK_START, N.y + 0.12f * wobble);

    float3 weight = pow(abs(N), BLEND_SHARPNESS);
    weight /= (weight.x + weight.y + weight.z);

    float3 albedo =
        planeColor(P.zy, grass) * weight.x +
        planeColor(P.xz, grass) * weight.y +
        planeColor(P.xy, grass) * weight.z;

    float3 toLight = normalize(toLight_ambient.xyz);
    float ambient = toLight_ambient.w;
    float ndotl = saturate(dot(N, toLight));

    float3 fill = lerp(GROUND_BOUNCE, SKY_FILL, 0.5f + 0.5f * N.y) * ambient;

    // The swapchain is UNORM, so this is the one encode; fs_debug_normal ships
    // a direction, not a color.
    return float4(linear2srgbApprox(albedo * (fill + ndotl)), 1.0);
}

[shader("fragment")]
float4 fs_debug_normal(VertexOut input) : SV_Target {
    float3 N = normalize(input.normal);

    return float4(0.5 * N + 0.5, 1);
}
