#pragma once

// Substrate's slab as far as a forward renderer without IBL or diffusion
// SSS follows it: a body slab under a coat slab, layered vertically

#include "PaintLook.slang"

// the lobes a debug toggle can take out
#define PAINT_LOBE_DIFFUSE (1u << 0)
#define PAINT_LOBE_SPECULAR (1u << 1)
#define PAINT_LOBE_HAZE (1u << 2)
#define PAINT_LOBE_FUZZ (1u << 3)
#define PAINT_LOBE_SSS (1u << 4)
#define PAINT_LOBE_COAT (1u << 5)
#define PAINT_LOBE_SKY (1u << 6)
#define PAINT_LOBE_ALL 0x7Fu

// what the legacy GBuffer would export, as Unreal's buffer view colors it
#define PAINT_EXPORT_DEFAULT_LIT 0u
#define PAINT_EXPORT_CLOTH 1u

static const float PaintPi = 3.14159265;

struct PaintSlabInputs {
    float3 diffuseAlbedo;
    float3 f0;
    float roughness;
    float secondRoughness;
    float secondWeight;
    float fuzz;
    float3 fuzzColor;
    float fuzzRoughness;
    float3 sssMfp;
    float sssScale;
    float coat;
    float coatRoughness;
    float3 normal;
};

// the slab Unreal builds from a look: metalness to diffuse albedo and F0
// (Specular 0.5 is F0 0.04), the style already applied
PaintSlabInputs ToSlab(PaintLook look, PaintStyled style, float coatRoughness) {
    PaintSlabInputs s;
    float metal = saturate(look.metallic);
    s.diffuseAlbedo = look.baseColor * (1.0 - metal);
    s.f0 = lerp(0.08 * look.specular.xxx, look.baseColor, metal);
    s.roughness = style.roughness;
    s.secondRoughness = look.secondRoughness;
    s.secondWeight = look.secondWeight;
    s.fuzz = style.fuzz;
    // the floor's fuzz takes the blended base color
    s.fuzzColor = look.baseColor;
    s.fuzzRoughness = look.fuzzRoughness;
    s.sssMfp = look.sssMfp;
    s.sssScale = look.sssScale;
    s.coat = style.coat;
    s.coatRoughness = coatRoughness;
    s.normal = look.normal;
    return s;
}

// the Blendable GBuffer's single closure as Unreal's source reads, not a
// capture: coat and haze blended in, diffusion SSS dropped
PaintSlabInputs Collapse(PaintSlabInputs s) {
    PaintSlabInputs o = s;
    o.roughness = lerp(s.roughness, s.secondRoughness, s.secondWeight);
    o.secondWeight = 0.0;
    o.roughness = lerp(o.roughness, s.coatRoughness, saturate(s.coat));
    o.f0 = lerp(s.f0, 0.04.xxx, saturate(s.coat));
    o.coat = 0.0;
    o.sssScale = 0.0;
    return o;
}

// the legacy model the collapsed slab exports: Cloth where there is fuzz
uint ExportModel(PaintSlabInputs s) {
    return s.fuzz > 0.0 ? PAINT_EXPORT_CLOTH : PAINT_EXPORT_DEFAULT_LIT;
}

float3 ExportModelColor(uint model) {
    switch(model) {
    case PAINT_EXPORT_CLOTH:
        return float3(0.7, 1.0, 1.0);
    }
    return float3(0.1, 1.0, 0.1);
}

float D_GGX(float a2, float NoH) {
    float d = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PaintPi * d * d);
}

// height-correlated Smith, as one visibility term
float V_SmithJoint(float a2, float NoV, float NoL) {
    float v = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float l = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / (v + l + 1e-5);
}

float3 F_Schlick(float3 f0, float VoH) {
    float fc = pow(1.0 - VoH, 5.0);
    return f0 + (1.0 - f0) * fc;
}

float specularLobe(float roughness, float NoH, float NoV, float NoL) {
    float a = max(roughness * roughness, 2e-3);
    float a2 = a * a;
    return D_GGX(a2, NoH) * V_SmithJoint(a2, NoV, NoL);
}

// Charlie sheen with Neubelt's visibility: the fuzz lobe
float fuzzLobe(float roughness, float NoH, float NoV, float NoL) {
    float r = max(roughness * roughness, 0.07);
    float sin2 = max(1.0 - NoH * NoH, 1e-4);
    float d = (2.0 + 1.0 / r) * pow(sin2, 0.5 / r) / (2.0 * PaintPi);
    float v = 1.0 / (4.0 * (NoL + NoV - NoL * NoV) + 1e-4);
    return d * v;
}

struct PaintLightTerms {
    float3 diffuse;
    float3 specular;
    float3 fuzz;
    float3 coat;
};

// one light's contribution, each lobe apart so the views can show them
PaintLightTerms EvalSlab(PaintSlabInputs s, float3 L, float3 V, float3 radiance, uint lobes) {
    PaintLightTerms t;
    float3 N = s.normal;
    float3 H = normalize(L + V);
    float NoL = dot(N, L);
    float NoV = max(dot(N, V), 1e-4);
    float NoH = saturate(dot(N, H));
    float VoH = saturate(dot(V, H));
    float NoLc = saturate(NoL);

    // the coat is the top slab: its specular first, then what it lets past
    float3 coatF = F_Schlick(0.04.xxx, VoH);
    float coatWeight = (lobes & PAINT_LOBE_COAT) != 0u ? saturate(s.coat) : 0.0;
    float3 throughCoat = (1.0 - coatWeight * F_Schlick(0.04.xxx, NoV)) *
                         (1.0 - coatWeight * F_Schlick(0.04.xxx, NoLc));
    t.coat = coatWeight * coatF * specularLobe(s.coatRoughness, NoH, NoV, NoLc) * NoLc * radiance;

    float3 F = F_Schlick(s.f0, VoH);
    float spec = specularLobe(s.roughness, NoH, NoV, NoLc);
    if((lobes & PAINT_LOBE_HAZE) != 0u) {
        // haze: a second, rougher lobe of the same F0, weighted in
        spec = lerp(spec, specularLobe(s.secondRoughness, NoH, NoV, NoLc), s.secondWeight);
    }
    t.specular = (lobes & PAINT_LOBE_SPECULAR) != 0u
        ? F * spec * NoLc * radiance * throughCoat
        : 0.0.xxx;

    // diffusion SSS has no forward equivalent: light wraps past the
    // terminator and picks up the mean free path's tint, a labelled stand-in
    float3 diffuse = s.diffuseAlbedo / PaintPi * NoLc;
    if((lobes & PAINT_LOBE_SSS) != 0u && s.sssScale > 0.0) {
        float wrap = 0.35 * saturate(s.sssScale);
        float wrapped = saturate((NoL + wrap) / (1.0 + wrap)) / (1.0 + wrap);
        float3 tint = lerp(1.0.xxx, normalize(s.sssMfp + 1e-3) * 1.4, 0.35);
        diffuse = s.diffuseAlbedo / PaintPi * lerp(NoLc.xxx, wrapped * tint, 0.6);
    }
    float fuzzWeight = (lobes & PAINT_LOBE_FUZZ) != 0u ? saturate(s.fuzz) : 0.0;
    t.diffuse = (lobes & PAINT_LOBE_DIFFUSE) != 0u
        ? diffuse * (1.0 - F) * (1.0 - 0.5 * fuzzWeight) * radiance * throughCoat
        : 0.0.xxx;
    t.fuzz = fuzzWeight * s.fuzzColor * fuzzLobe(s.fuzzRoughness, NoH, NoV, NoLc) * NoLc * radiance * throughCoat;

    return t;
}

// the sky's glossy reflection: no IBL here, so the view's hemisphere stands
// in for it, through Karis's analytic environment BRDF
float3 SkyReflection(PaintSlabInputs s, float3 V, float3 sky, float3 ground, uint lobes) {
    if((lobes & PAINT_LOBE_SKY) == 0u)
        return 0.0.xxx;
    float3 N = s.normal;
    float NoV = max(dot(N, V), 1e-4);
    float3 R = reflect(-V, N);
    float3 env = lerp(ground, sky, saturate(R.y * 0.5 + 0.5));

    const float4 c0 = float4(-1.0, -0.0275, -0.572, 0.022);
    const float4 c1 = float4(1.0, 0.0425, 1.04, -0.04);
    float4 r = s.roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
    float2 ab = float2(-1.04, 1.04) * a004 + r.zw;
    float3 body = (lobes & PAINT_LOBE_SPECULAR) != 0u ? env * (s.f0 * ab.x + ab.y) : 0.0.xxx;

    float coatWeight = (lobes & PAINT_LOBE_COAT) != 0u ? saturate(s.coat) : 0.0;
    float4 rc = s.coatRoughness * c0 + c1;
    float ac = min(rc.x * rc.x, exp2(-9.28 * NoV)) * rc.x + rc.y;
    float2 abc = float2(-1.04, 1.04) * ac + rc.zw;
    float3 coat = coatWeight * env * (0.04 * abc.x + abc.y);

    return body * (1.0 - coatWeight * F_Schlick(0.04.xxx, NoV)) + coat;
}
