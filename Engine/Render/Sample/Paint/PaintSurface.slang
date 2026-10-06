// The painted surfaces, drawn in the paint hook pass with the engine's lights
// and shadow map: MF_PaintOverlay's atlas lookup, the team looks laid over
// the bare one, the relief normal, the slab's lobes; or one of the data views
// the topics are shown with.

#include "Lighting.slang"
#include "PaintCommon.slang"
#include "PaintHeight.slang"
#include "PaintLook.slang"
#include "PaintSlab.slang"
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
    // the score's cell ids, one per (voxel, direction), in a score view
    DescriptorHandle<StructuredBuffer<uint>> cellIds;
    uint cellBase;
    uint cellPad;
    // xyz the grid's scaled-local origin, w its cell size
    float4 cellOrigin;
    // xyz voxel counts, w 1 when the ids are bound
    uint4 cellDims;
    // x lobes, y edge mode, z team blend, w height filter
    uint4 lookModes;
    // x height from ids, y edge fade on, z collapsed, w unused
    uint4 lookModes2;
    // MPC_PaintStyle: x coat scale, y fuzz scale, z roughness bias, w flow
    float4 lookStyle;
    // x normal strength, y coat roughness, z thin height, w thin min opacity
    float4 lookStyle2;
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

// the paint id the CPU score gave the cell under this point: its voxel in
// the scaled-local frame, its direction from the mesh normal
uint scoreCellId(float3 localPos, float3 localNormal, out float3 cellCoord) {
    float3 p = localPos * scale3D.xyz;
    cellCoord = (p - cellOrigin.xyz) / cellOrigin.w;
    if(cellDims.w == 0u)
        return PAINT_ID_NONE;
    int3 last = int3(cellDims.xyz) - 1;
    int3 v = clamp(int3(floor(cellCoord)), int3(0, 0, 0), last);
    uint direction = IslandDirection(localNormal);
    uint index = ((uint(v.z) * cellDims.y + uint(v.y)) * cellDims.x + uint(v.x)) * 6u + direction;
    StructuredBuffer<uint> ids = cellIds;

    return ids[cellBase + index];
}

// a thin dark line on every cell border, in the surface's own plane
float cellBorder(float3 cellCoord, uint direction) {
    float2 c = direction < 2u ? cellCoord.yz : (direction < 4u ? cellCoord.xz : cellCoord.xy);
    float2 f = abs(frac(c) - 0.5);
    float2 w = fwidth(c);
    return max(step(0.5 - w.x * 1.2, f.x), step(0.5 - w.y * 1.2, f.y));
}

// mesh space to the Crowy world for a vector the scale already divided once:
// rotate, multiply the scale back, then the boundary's (y, z, x)
float3 meshToWorldNormal(float3 v) {
    float3 s = v * scale3D.xyz;
    float3 mint = float3(dot(rotation0.xyz, s), dot(rotation1.xyz, s), dot(rotation2.xyz, s));
    return normalize(float3(mint.y, mint.z, mint.x));
}

// everything a pixel's look needs, read once whichever view shows it
struct SurfacePoint {
    float2 uv;
    uint direction;
    bool kept;
    uint id;
    float4 paint;
    float4 sd;
    float4 coverage;
    PaintHeightSample height;
    float3 vertexNormal;
    float3 paintNormal;
};

SurfacePoint readSurface(SurfaceOut input, bool needsHeight) {
    SurfacePoint p;
    float3 n = (input.localPos - boundsMin.xyz) / max(boundsSize.xyz, 1e-4);
    p.direction = IslandDirection(input.localNormal);
    float4 island = islands[p.direction];
    p.kept = island.z > 0.0;
    p.uv = IslandUV(n, island, p.direction, atlas.y);

    Texture2D paintTex = tableTexture(maps.x);
    Texture2D positionTex = tableTexture(maps.y);
    Texture2D fadeTex = tableTexture(maps.z);
    p.paint = paintTex.SampleLevel(nearestClamp, p.uv, 0);
    p.id = DecodePaintId(p.paint.r);
    p.coverage = TeamCoverage(paintTex, p.uv, atlas.y, atlas.w, lookModes.y, p.sd);

    // the vertex normal undone of the scale once, as UE's normal reaches
    // the shader; the relief normal is in the same mesh space
    float3 meshNormal = normalize(input.localNormal / scale3D.xyz);
    p.vertexNormal = meshToWorldNormal(meshNormal / scale3D.xyz);
    p.paintNormal = p.vertexNormal;
    p.height.height = 0.0;
    p.height.mask = 0.0;
    p.height.thickness = 0.0;
    p.height.du = 0.0;
    p.height.dv = 0.0;
    if(needsHeight && p.kept) {
        p.height = PaintHeightField(paintTex, fadeTex, p.uv, atlas.y, atlas.w, lookStyle.w,
                                    lookModes.w, lookModes2.x != 0u);
        if(lookModes2.y == 0u) {
            // the island-edge fade off: the height runs straight to the edge
            float fade = fadeTex.SampleLevel(linearClamp, p.uv, 0).r;
            p.height.height = p.height.thickness;
            p.height.du = p.height.du / max(fade, 1e-3);
            p.height.dv = p.height.dv / max(fade, 1e-3);
        }
        float3 relief = PaintNormal(positionTex, fadeTex, p.uv, atlas.y, boundsSize.xyz,
                                    meshNormal, scale3D.xyz, p.height.du, p.height.dv,
                                    PAINT_MAX_HEIGHT, lookStyle2.x);
        p.paintNormal = meshToWorldNormal(relief);
    }
    return p;
}

// the lit paint: look, style, slab, every light, the sky
float3 shadeLit(SurfaceOut input, SurfacePoint p, out PaintSlabInputs slab) {
    PaintLook base = PlainLook(p.vertexNormal);
    // MF_PaintOverlay's thin-paint opacity; with the shipped minimum of 1
    // paint never turns see-through
    float thin = lerp(lookStyle2.w, 1.0, smoothstep(0.0, lookStyle2.z, p.height.thickness));
    float consumed;
    PaintLook look = p.kept
        ? BlendTeams(base, p.coverage, p.paintNormal, lookModes.z, thin, consumed)
        : base;
    look.normal = normalize(look.normal);
    PaintStyled style = ApplyStyle(look, lookStyle.x, lookStyle.y, lookStyle.z);
    slab = ToSlab(look, style, lookStyle2.y);
    if(lookModes2.z != 0u)
        slab = Collapse(slab);

    float3 V = normalize(cameraPosition.xyz - input.posWorld);
    uint lobes = lookModes.x;
    float3 color = 0.0.xxx;
    for(uint i = 0; i < LightCount(); ++i) {
        LightSample light = SampleLight(i, input.posWorld, p.vertexNormal);
        PaintLightTerms t = EvalSlab(slab, light.direction, V, light.radiance * light.attenuation, lobes);
        color += t.diffuse + t.specular + t.fuzz + t.coat;
    }
    float3 ambient = (lobes & PAINT_LOBE_DIFFUSE) != 0u
        ? AmbientIrradiance(slab.normal) * slab.diffuseAlbedo
        : 0.0.xxx;

    return color + ambient + SkyReflection(slab, V, skyAmbient.xyz, groundAmbient.xyz, lobes);
}

bool viewNeedsHeight(uint view) {
    return view == PAINT_VIEW_LIT || view == PAINT_VIEW_HEIGHT_FIELD ||
           view == PAINT_VIEW_NORMAL || view == PAINT_VIEW_BASE_COLOR ||
           view == PAINT_VIEW_ROUGHNESS || view == PAINT_VIEW_SHADING_MODEL;
}

[shader("fragment")]
float4 fs_surface(SurfaceOut input) : SV_Target {
    uint view = input.posProj.x < screen.x ? modes.x : modes.y;
    SurfacePoint p = readSurface(input, viewNeedsHeight(view));
    Texture2D positionTex = tableTexture(maps.y);
    Texture2D fadeTex = tableTexture(maps.z);
    int3 texel = int3(int2(floor(p.uv * atlas.x)), 0);
    int last = int(atlas.x) - 1;
    texel.xy = clamp(texel.xy, int2(0, 0), int2(last, last));

    float3 display;
    switch(view) {
    case PAINT_VIEW_ISLANDS: {
        if(!p.kept) {
            // a direction this surface keeps no paint on
            float stripe = frac((input.posProj.x + input.posProj.y) / 24.0);
            display = stripe < 0.5 ? float3(0.24, 0.24, 0.26) : float3(0.32, 0.32, 0.34);
            break;
        }
        float2 block = floor(p.uv * atlas.x / 16.0);
        float checker = fmod(block.x + block.y, 2.0);
        display = DirectionColor(p.direction) * (checker > 0.5 ? 0.82 : 1.0);
        break;
    }
    case PAINT_VIEW_PAINT_ID:
        display = TeamDisplayColor(p.id);
        break;
    case PAINT_VIEW_HEIGHT:
        display = RampColor(p.paint.g);
        break;
    case PAINT_VIEW_DISTANCE:
        display = RampColor(p.paint.b);
        break;
    case PAINT_VIEW_POSITION:
        display = saturate(positionTex.Load(texel).xyz);
        break;
    case PAINT_VIEW_EDGE_FADE:
        display = RampColor(fadeTex.SampleLevel(linearClamp, p.uv, 0).r);
        break;
    case PAINT_VIEW_SCORE: {
        float3 cellCoord;
        uint owner = scoreCellId(input.localPos, input.localNormal, cellCoord);
        display = owner < 4u ? TeamDisplayColor(owner) : float3(0.78, 0.77, 0.75);
        display *= 1.0 - 0.35 * cellBorder(cellCoord, p.direction);
        break;
    }
    case PAINT_VIEW_DIVERGENCE: {
        // where the picture and the score agree the team colour; where only
        // the picture has paint amber, only the score violet
        float3 cellCoord;
        uint owner = scoreCellId(input.localPos, input.localNormal, cellCoord);
        bool pictured = p.id < 4u;
        bool scored = owner < 4u;
        float stripe = frac((input.posProj.x - input.posProj.y) / 10.0) < 0.5 ? 1.0 : 0.8;
        if(pictured && scored && owner == p.id)
            display = TeamDisplayColor(p.id);
        else if(pictured)
            display = float3(0.96, 0.72, 0.20) * stripe;
        else if(scored)
            display = float3(0.56, 0.38, 0.92) * stripe;
        else
            display = float3(0.80, 0.79, 0.77);
        display *= 1.0 - 0.25 * cellBorder(cellCoord, p.direction);
        break;
    }
    case PAINT_VIEW_SIGNED_DISTANCE: {
        // the strongest team's distance: red outside, mint inside, white on
        // the edge, isolines every texel
        float sd = max(max(p.sd.x, p.sd.y), max(p.sd.z, p.sd.w));
        float t = saturate(sd / atlas.w * 0.5 + 0.5);
        display = lerp(float3(0.62, 0.20, 0.18), float3(0.12, 0.66, 0.56), t);
        float iso = abs(frac(sd + 0.5) - 0.5);
        if(iso < fwidth(sd) * 0.8)
            display *= 0.7;
        if(abs(sd) < fwidth(sd) * 1.2)
            display = float3(0.97, 0.97, 0.95);
        break;
    }
    case PAINT_VIEW_COVERAGE: {
        float c = saturate(p.coverage.x + p.coverage.y + p.coverage.z + p.coverage.w);
        display = RampColor(c);
        break;
    }
    case PAINT_VIEW_HEIGHT_FIELD:
        display = RampColor(p.height.height);
        break;
    case PAINT_VIEW_NORMAL: {
        PaintSlabInputs slab;
        shadeLit(input, p, slab);
        display = slab.normal * 0.5 + 0.5;
        break;
    }
    case PAINT_VIEW_BASE_COLOR: {
        PaintSlabInputs slab;
        shadeLit(input, p, slab);
        display = linear2srgb(saturate(slab.diffuseAlbedo));
        break;
    }
    case PAINT_VIEW_ROUGHNESS: {
        PaintSlabInputs slab;
        shadeLit(input, p, slab);
        display = slab.roughness.xxx;
        break;
    }
    case PAINT_VIEW_SHADING_MODEL: {
        PaintSlabInputs slab;
        shadeLit(input, p, slab);
        // the model the Blendable GBuffer would export, collapsed or not
        display = ExportModelColor(Collapse(slab).exportModel);
        break;
    }
    default: {
        PaintSlabInputs slab;
        return float4(shadeLit(input, p, slab), 1.0);
    }
    }

    return float4(DisplayToRadiance(display), 1.0);
}
