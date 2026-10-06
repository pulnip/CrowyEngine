#pragma once

// MintChoco's look before light: per-team signed distance at the edge, the
// team looks laid over the bare one by coverage, and the shared style

#include "PaintCommon.slang"
#include "Sampler.slang"

// how the edge between paint and not is read
#define PAINT_EDGE_NEAREST 0u
#define PAINT_EDGE_BILINEAR 1u
#define PAINT_EDGE_SIGNED_DISTANCE 2u

// how teams are laid over each other
#define PAINT_BLEND_NAIVE 0u
#define PAINT_BLEND_CONSUMED 1u

// what a material layer carries through the stack: a look's attributes
struct PaintLook {
    float3 baseColor;
    float specular;
    float roughness;
    float metallic;
    float3 sssMfp;
    float sssScale;
    float fuzzAmount;
    float fuzzRoughness;
    float secondRoughness;
    float secondWeight;
    float wetCoat;
    float3 normal;
};

PaintLook lerpLook(PaintLook a, PaintLook b, float t) {
    PaintLook o;
    o.baseColor = lerp(a.baseColor, b.baseColor, t);
    o.specular = lerp(a.specular, b.specular, t);
    o.roughness = lerp(a.roughness, b.roughness, t);
    o.metallic = lerp(a.metallic, b.metallic, t);
    o.sssMfp = lerp(a.sssMfp, b.sssMfp, t);
    o.sssScale = lerp(a.sssScale, b.sssScale, t);
    o.fuzzAmount = lerp(a.fuzzAmount, b.fuzzAmount, t);
    o.fuzzRoughness = lerp(a.fuzzRoughness, b.fuzzRoughness, t);
    o.secondRoughness = lerp(a.secondRoughness, b.secondRoughness, t);
    o.secondWeight = lerp(a.secondWeight, b.secondWeight, t);
    o.wetCoat = lerp(a.wetCoat, b.wetCoat, t);
    o.normal = lerp(a.normal, b.normal, t);
    return o;
}

// ML_Look_Plain: the shipped blocks' bare surface; its own flat normal map
// overwrites the paint normal under it
PaintLook PlainLook(float3 vertexNormal) {
    PaintLook o;
    o.baseColor = float3(0.72, 0.70, 0.66);
    o.specular = 0.5;
    o.roughness = 0.6;
    o.metallic = 0.0;
    o.sssMfp = float3(0.0, 0.0, 0.0);
    o.sssScale = 0.0;
    o.fuzzAmount = 0.0;
    o.fuzzRoughness = 0.5;
    o.secondRoughness = 0.9;
    o.secondWeight = 0.0;
    o.wetCoat = 0.0;
    o.normal = vertexNormal;
    return o;
}

// MPC_TeamLook through MF_TeamLook: 0 and 2 Mint, 1 and 3 Choco; the paint
// keeps the relief normal it came in with
PaintLook TeamLook(uint team, float3 paintNormal) {
    PaintLook o;
    bool mint = (team & 1u) == 0u;
    o.baseColor = mint ? float3(0.0, 0.8, 0.505794) : float3(0.158, 0.04661, 0.01738);
    o.sssMfp = mint ? float3(0.08445, 0.563, 0.38701) : float3(0.088, 0.032931, 0.01848);
    o.roughness = mint ? 0.5 : 0.45;
    o.specular = mint ? 0.45 : 0.25;
    o.metallic = 0.0;
    o.wetCoat = mint ? 0.1 : 0.0;
    o.secondRoughness = mint ? 0.8 : 0.95;
    o.secondWeight = mint ? 0.5 : 0.4;
    o.fuzzAmount = mint ? 0.3 : 0.0;
    o.fuzzRoughness = mint ? 0.7 : 0.5;
    // SSSMFPScale, the looks' one layer parameter
    o.sssScale = 1.0;
    o.normal = paintNormal;
    return o;
}

// MF_PaintCorners: teams in xyzw, positive inside, in texels; the bilinear
// zero lands on the sub-texel edge the brush drew
float4 PaintCorners(Texture2D Tex, float2 UV, float TexelSize, float DistRange) {
    const float4 Teams = float4(0.0, 1.0, 2.0, 3.0);
    float2 tc = UV / TexelSize - 0.5;
    float2 f = frac(tc);
    float2 baseUV = (floor(tc) + 0.5) * TexelSize;
    float3 s00 = Tex.SampleLevel(nearestClamp, baseUV, 0).rgb;
    float3 s10 = Tex.SampleLevel(nearestClamp, baseUV + float2(TexelSize, 0.0), 0).rgb;
    float3 s01 = Tex.SampleLevel(nearestClamp, baseUV + float2(0.0, TexelSize), 0).rgb;
    float3 s11 = Tex.SampleLevel(nearestClamp, baseUV + float2(TexelSize, TexelSize), 0).rgb;
    // R carries id + 8 * star generation; only the id matters here
    float4 m00 = 1.0 - step(0.5, abs(Teams - fmod(round(s00.r * 255.0), 8.0)));
    float4 m10 = 1.0 - step(0.5, abs(Teams - fmod(round(s10.r * 255.0), 8.0)));
    float4 m01 = 1.0 - step(0.5, abs(Teams - fmod(round(s01.r * 255.0), 8.0)));
    float4 m11 = 1.0 - step(0.5, abs(Teams - fmod(round(s11.r * 255.0), 8.0)));
    float4 d00 = (2.0 * m00 - 1.0) * (1.0 - s00.b) * DistRange;
    float4 d10 = (2.0 * m10 - 1.0) * (1.0 - s10.b) * DistRange;
    float4 d01 = (2.0 * m01 - 1.0) * (1.0 - s01.b) * DistRange;
    float4 d11 = (2.0 * m11 - 1.0) * (1.0 - s11.b) * DistRange;
    float4 sd = lerp(lerp(d00, d10, f.x), lerp(d01, d11, f.x), f.y);
    float4 agree = m00 + m10 + m01 + m11;
    sd = lerp(sd, DistRange, step(3.5, agree));
    sd = lerp(sd, -DistRange, 1.0 - step(0.5, agree));
    return sd;
}

// each team's coverage at this pixel, by the edge mode the data views compare
float4 TeamCoverage(Texture2D Tex, float2 UV, float TexelSize, float DistRange, uint edgeMode, out float4 sd) {
    sd = PaintCorners(Tex, UV, TexelSize, DistRange);
    if(edgeMode == PAINT_EDGE_NEAREST) {
        uint id = DecodePaintId(Tex.SampleLevel(nearestClamp, UV, 0).r);
        return float4(id == 0u ? 1.0 : 0.0, id == 1u ? 1.0 : 0.0,
                      id == 2u ? 1.0 : 0.0, id == 3u ? 1.0 : 0.0);
    }
    if(edgeMode == PAINT_EDGE_BILINEAR) {
        // what a bilinear read of an id map would do: it blends the bytes,
        // so between team 1 and none it invents the ids in between
        float r = Tex.SampleLevel(linearClamp, UV, 0).r;
        uint id = uint(round(r * 255.0)) & 7u;
        return float4(id == 0u ? 1.0 : 0.0, id == 1u ? 1.0 : 0.0,
                      id == 2u ? 1.0 : 0.0, id == 3u ? 1.0 : 0.0);
    }

    // MF_PaintTeamBlend's band: one pixel wide, capped at one texel, so
    // minification never smears the background through the rim
    float4 cov;
    for(uint t = 0u; t < 4u; ++t) {
        float w = clamp(0.5 * fwidth(sd[t]), 1e-3, 0.5);
        cov[t] = smoothstep(-w, w, sd[t]);
    }
    return cov;
}

// MF_PaintTeamBlend: dividing by what earlier teams left over keeps the
// base from showing through where two teams meet
PaintLook BlendTeams(PaintLook base, float4 cov, float3 paintNormal, uint blend, float thin) {
    PaintLook o = base;
    float S = 0.0;
    for(uint t = 0u; t < 4u; ++t) {
        float c = cov[t] * thin;
        float alpha = blend == PAINT_BLEND_NAIVE
            ? c
            : saturate(c / max(1.0 - S, 1e-4));
        o = lerpLook(o, TeamLook(t, paintNormal), alpha);
        S = saturate(S + c);
    }
    return o;
}

// MF_PaintStyle over MPC_PaintStyle: coat from the wet coat, fuzz scaled,
// roughness biased
struct PaintStyled {
    float coat;
    float fuzz;
    float roughness;
};

PaintStyled ApplyStyle(PaintLook look, float coatScale, float fuzzScale, float roughnessBias) {
    PaintStyled o;
    o.coat = look.wetCoat * coatScale;
    o.fuzz = look.fuzzAmount * fuzzScale;
    o.roughness = saturate(look.roughness + roughnessBias);
    return o;
}
