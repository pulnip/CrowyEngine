// The sea's material: a grid lifted by the longer waves, shaded by as many as
// a pixel resolves; lanes: custom0.x the loop's step, custom1.xyz the moon.

#include "ForwardShading.slang"
#include "NightSky.slang"
#include "Random.slang"
#include "Sea.slang"

static const float3 DeepWater = float3(0.006, 0.018, 0.03);
static const float3 Foam = float3(0.55, 0.6, 0.65);

uint seaStep(MaterialData m) { return uint(m.custom0.x); }

// smooth noise on the integer hash, one draw a unit cell
float latticeNoise(float2 p) {
    float2 cell = floor(p);
    float2 f = p - cell;
    f = f * f * (3.0 - 2.0 * f);
    uint x = uint(int(cell.x));
    uint z = uint(int(cell.y));
    float n00 = float(pcgHash(x ^ pcgHash(z)) >> 8u) / 16777216.0;
    float n10 = float(pcgHash((x + 1u) ^ pcgHash(z)) >> 8u) / 16777216.0;
    float n01 = float(pcgHash(x ^ pcgHash(z + 1u)) >> 8u) / 16777216.0;
    float n11 = float(pcgHash((x + 1u) ^ pcgHash(z + 1u)) >> 8u) / 16777216.0;
    return lerp(lerp(n00, n10, f.x), lerp(n01, n11, f.x), f.y);
}

// the standard vertex lifted by the sea's grid waves; normals come per pixel
[shader("vertex")]
VertexOut vs_ocean(
    uint vertexID : SV_VertexID,
    uint drawID : SV_StartInstanceLocation
){
    DrawData d = LoadDraw(drawID);
    Vertex input = LoadVertex(d, vertexID);
    MaterialData m = LoadMaterial(d.materialIndex);

    float4 posWorld = mul(d.world, float4(input.position, 1.0));
    posWorld.y +=
        seaHeight(posWorld.xz, seaStep(m), SEA_VERTEX_WAVES, true);

    VertexOut output;
    output.posProj = mul(viewProj, posWorld);
    output.posWorld = posWorld.xyz;
    output.normal = float3(0.0, 1.0, 0.0);
    output.texCoord = input.texCoord;
    output.materialIndex = d.materialIndex;

    return output;
}

[shader("fragment")]
float4 fs_ocean(VertexOut v, bool frontFace : SV_IsFrontFace) : SV_Target {
    MaterialData m = LoadMaterial(v.materialIndex);
    uint step = seaStep(m);
    Surface s = loadSurface(v, frontFace);

    float3 toCamera = cameraPosition.xyz - v.posWorld;
    float away = length(toCamera);
    float3 V = toCamera / away;
    // a pixel's width on the water here, and the waves still wider than four
    float footprint = away * ISLAND_PIXEL_ANGLE / max(V.y, 0.1);
    uint count = SEA_WAVE_COUNT;
    for(uint i = 0u; i < SEA_WAVE_COUNT; ++i) {
        if(1.0 / SeaInvLength[i] < 4.0 * footprint) {
            count = i;
            break;
        }
    }
    float eps = clamp(0.5 * footprint, 0.005, 0.5);
    float2 xz = v.posWorld.xz;
    float height = seaHeight(xz, step, count, false);
    float east = seaHeight(xz + float2(eps, 0.0), step, count, false);
    float north = seaHeight(xz + float2(0.0, eps), step, count, false);
    float3 N = normalize(
        float3(-(east - height) / eps, 1.0, -(north - height) / eps)
    );

    // the waves too fine to draw here roughen the surface instead
    float dropped = float(SEA_WAVE_COUNT - count) / float(SEA_WAVE_COUNT);
    s.normal = N;
    s.metallic = 0.0;
    s.roughness = lerp(0.06, 0.3, dropped);
    s.albedo = DeepWater * (1.0 + 1.5 * saturate(height / 0.25));
    float3 R = reflect(-V, N);
    R.y = max(R.y, 0.005);
    float fresnel = 0.04 + 0.96 * pow(1.0 - saturate(dot(N, V)), 5.0);
    s.emissive = fresnel * nightSky(normalize(R), m.custom1.xyz, false);

    // foam where the sea runs thin over the sand, in lines that run up it
    float over = height - islandCrown(xz);
    float band = 1.0 - smoothstep(0.0, 0.25, over);
    float still = -islandCrown(xz);
    float lines =
        0.5 + 0.5 * sin(6.2831853 * (still / 0.35 + loopPhase(step, 24u)));
    float foam = band * saturate(lines * latticeNoise(xz * 2.0) * 1.5);
    s.albedo = lerp(s.albedo, Foam, foam * 0.8);
    s.roughness = lerp(s.roughness, 0.9, foam);
    s.emissive *= 1.0 - foam;

    float3 color = sceneColor(s, v);
    // near the horizon and toward the grid's rim, the sea meets the far sea
    // the sky draws under the horizon
    if(debugMode == DebugModeLit) {
        float nearHorizon = 1.0 - smoothstep(0.004, 0.012, V.y);
        float rim = smoothstep(200.0, 280.0, length(xz));
        color = lerp(color, FarSea, max(nearHorizon, rim));
    }
    return float4(color, 1.0);
}
