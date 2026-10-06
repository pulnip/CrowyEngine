#pragma once

// MF_PaintHeightField and MF_PaintNormal: the height read through a
// B-spline that hands back its own slope, and the normal built from it

#include "PaintCommon.slang"
#include "Sampler.slang"

// how G is read back
#define PAINT_HEIGHT_NEAREST 0u
#define PAINT_HEIGHT_BILINEAR 1u
#define PAINT_HEIGHT_BSPLINE 2u

// where the slope comes from
#define PAINT_NORMAL_FINITE_DIFFERENCE 0u
#define PAINT_NORMAL_ANALYTIC 1u

struct PaintHeightSample {
    // filtered height x paint mask x edge fade, the displacement's 0..1
    float height;
    // without the island fade, for the thin-paint see-through
    float thickness;
    // the slope per texel along the atlas axes, the fade carried through
    float du;
    float dv;
};

// the value the field filters: the accumulated G, or the old relief that
// came from the painted area alone
float heightTap(Texture2D Tex, float2 uv, bool fromIds) {
    float4 s = Tex.SampleLevel(nearestClamp, uv, 0);
    return fromIds ? (DecodePaintId(s.r) < 7u ? 1.0 : 0.0) : s.g;
}

float bilinearHeight(Texture2D Tex, float2 UV, float TexelSize, bool fromIds) {
    float2 tc = UV / TexelSize - 0.5;
    float2 f = frac(tc);
    float2 baseUV = (floor(tc) + 0.5) * TexelSize;
    float h00 = heightTap(Tex, baseUV, fromIds);
    float h10 = heightTap(Tex, baseUV + float2(TexelSize, 0.0), fromIds);
    float h01 = heightTap(Tex, baseUV + float2(0.0, TexelSize), fromIds);
    float h11 = heightTap(Tex, baseUV + float2(TexelSize, TexelSize), fromIds);
    return lerp(lerp(h00, h10, f.x), lerp(h01, h11, f.x), f.y);
}

// MF_PaintHeightField's Custom node, the B-spline branch verbatim but for
// syntax; `filter` and `fromIds` add the readings the topic compares it to
PaintHeightSample PaintHeightField(
    Texture2D Tex,
    Texture2D Fade,
    float2 UV,
    float TexelSize,
    float DistRange,
    float Flow,
    uint filter,
    bool fromIds,
    bool edgeFade
) {
    const float FlowMaxTexels = 1.5;
    float invTexel = 1.0 / TexelSize;
    float2 tc = UV * invTexel - 0.5;
    float2 f = frac(tc);
    float2 baseUV = (floor(tc) + 0.5) * TexelSize;
    float2 du = float2(TexelSize, 0.0);
    float2 dv = float2(0.0, TexelSize);
    // the outline stays texel-sharp: the id and the edge distance are never
    // filtered, only the height is
    float3 s00 = Tex.SampleLevel(nearestClamp, baseUV, 0).rgb;
    float3 s10 = Tex.SampleLevel(nearestClamp, baseUV + du, 0).rgb;
    float3 s01 = Tex.SampleLevel(nearestClamp, baseUV + dv, 0).rgb;
    float3 s11 = Tex.SampleLevel(nearestClamp, baseUV + du + dv, 0).rgb;
    float m00 = (fmod(round(s00.r * 255.0), 8.0) < 6.5) ? 1.0 : 0.0;
    float m10 = (fmod(round(s10.r * 255.0), 8.0) < 6.5) ? 1.0 : 0.0;
    float m01 = (fmod(round(s01.r * 255.0), 8.0) < 6.5) ? 1.0 : 0.0;
    float m11 = (fmod(round(s11.r * 255.0), 8.0) < 6.5) ? 1.0 : 0.0;
    float sd = lerp(lerp((2.0 * m00 - 1.0) * (1.0 - s00.b), (2.0 * m10 - 1.0) * (1.0 - s10.b), f.x),
                    lerp((2.0 * m01 - 1.0) * (1.0 - s01.b), (2.0 * m11 - 1.0) * (1.0 - s11.b), f.x), f.y) * DistRange;
    float agree = m00 + m10 + m01 + m11;
    float mask = (agree > 3.5) ? 1.0 : ((agree < 0.5) ? 0.0 : saturate(sd + 0.5));

    float height;
    float hu;
    float hv;
    if(filter == PAINT_HEIGHT_BSPLINE) {
        // height and slope from one B-spline, so the normal matches the
        // silhouette; a fixed 12x12 window keeps the weights in registers
        float s = clamp(1.0 + Flow * FlowMaxTexels, 1.0, 2.5);
        float wx[12], gx[12], wy[12], gy[12];
        [unroll]
        for(int k = 0; k < 12; ++k) {
            float ox = (float(k) - 5.0 - f.x) / s;
            float ax = abs(ox);
            float sx = (ox < 0.0) ? -1.0 : 1.0;
            wx[k] = (ax < 1.0) ? (0.5 * ax * ax * ax - ax * ax + 2.0 / 3.0) : ((ax < 2.0) ? ((2.0 - ax) * (2.0 - ax) * (2.0 - ax) / 6.0) : 0.0);
            gx[k] = -((ax < 1.0) ? (sx * (1.5 * ax * ax - 2.0 * ax)) : ((ax < 2.0) ? (-sx * (2.0 - ax) * (2.0 - ax) * 0.5) : 0.0));
            float oy = (float(k) - 5.0 - f.y) / s;
            float ay = abs(oy);
            float sy = (oy < 0.0) ? -1.0 : 1.0;
            wy[k] = (ay < 1.0) ? (0.5 * ay * ay * ay - ay * ay + 2.0 / 3.0) : ((ay < 2.0) ? ((2.0 - ay) * (2.0 - ay) * (2.0 - ay) / 6.0) : 0.0);
            gy[k] = -((ay < 1.0) ? (sy * (1.5 * ay * ay - 2.0 * ay)) : ((ay < 2.0) ? (-sy * (2.0 - ay) * (2.0 - ay) * 0.5) : 0.0));
        }
        float W = 0.0, S = 0.0, Wu = 0.0, Su = 0.0, Wv = 0.0, Sv = 0.0;
        [unroll]
        for(int j = 0; j < 12; ++j) {
            [unroll]
            for(int i = 0; i < 12; ++i) {
                float g = heightTap(Tex, baseUV + (float(i) - 5.0) * du + (float(j) - 5.0) * dv, fromIds);
                float w = wx[i] * wy[j];
                float wu = gx[i] * wy[j];
                float wv = wx[i] * gy[j];
                W += w; S += w * g;
                Wu += wu; Su += wu * g;
                Wv += wv; Sv += wv * g;
            }
        }
        float invW = 1.0 / max(W, 1e-6);
        height = S * invW;
        hu = (Su - height * Wu) * invW / s;
        hv = (Sv - height * Wv) * invW / s;
    } else {
        // the readings the spline replaced, sloped by a central difference
        // one texel either side
        if(filter == PAINT_HEIGHT_NEAREST) {
            height = heightTap(Tex, UV, fromIds);
            hu = (heightTap(Tex, UV + du, fromIds) - heightTap(Tex, UV - du, fromIds)) * 0.5;
            hv = (heightTap(Tex, UV + dv, fromIds) - heightTap(Tex, UV - dv, fromIds)) * 0.5;
        } else {
            height = bilinearHeight(Tex, UV, TexelSize, fromIds);
            hu = (bilinearHeight(Tex, UV + du, TexelSize, fromIds) - bilinearHeight(Tex, UV - du, TexelSize, fromIds)) * 0.5;
            hv = (bilinearHeight(Tex, UV + dv, TexelSize, fromIds) - bilinearHeight(Tex, UV - dv, TexelSize, fromIds)) * 0.5;
        }
    }

    // the fade enters the slope by the product rule, so the height dies out
    // at island edges, where faces rise along different normals
    float fade = 1.0;
    float fu = 0.0;
    float fv = 0.0;
    if(edgeFade) {
        fade = Fade.SampleLevel(linearClamp, UV, 0).r;
        fu = (Fade.SampleLevel(linearClamp, UV + du, 0).r - Fade.SampleLevel(linearClamp, UV - du, 0).r) * 0.5;
        fv = (Fade.SampleLevel(linearClamp, UV + dv, 0).r - Fade.SampleLevel(linearClamp, UV - dv, 0).r) * 0.5;
    }

    PaintHeightSample o;
    o.height = height * mask * fade;
    o.thickness = height * mask;
    o.du = fade * hu + height * fu;
    o.dv = fade * hv + height * fv;
    return o;
}

// MF_PaintNormal's Custom node: `n` the unit mesh-space normal, S the
// scale; the result is divided by S once more on its way out (normalFix)
float3 PaintNormal(
    Texture2D Pos,
    Texture2D Fade,
    float2 UV,
    float TexelSize,
    float3 BoundsSize,
    float3 n,
    float3 S,
    float HeightDU,
    float HeightDV,
    float MaxHeight,
    float Strength,
    bool edgeFade
) {
    float invTexel = 1.0 / TexelSize;
    float2 du = float2(TexelSize, 0.0);
    float2 dv = float2(0.0, TexelSize);
    float3 normalFix = 1.0 / (S * S);
    // displacement runs in local units along n, so a world height of
    // MaxHeight is this much in local units
    float localPerWorld = 1.0 / max(length(S * n), 1e-3);

    // the outer fade is the second damping that keeps the edge slope from
    // sparkling
    float fadeC = edgeFade ? Fade.SampleLevel(linearClamp, UV, 0).r : 1.0;
    float hScale = MaxHeight * Strength * fadeC * localPerWorld;
    float hu = HeightDU * hScale;
    float hv = HeightDV * hScale;
    float2 baseUV = (floor(UV * invTexel - 0.5) + 0.5) * TexelSize;
    float3 pu = (Pos.SampleLevel(nearestClamp, baseUV + du, 0).xyz - Pos.SampleLevel(nearestClamp, baseUV - du, 0).xyz) * 0.5 * BoundsSize;
    float3 pv = (Pos.SampleLevel(nearestClamp, baseUV + dv, 0).xyz - Pos.SampleLevel(nearestClamp, baseUV - dv, 0).xyz) * 0.5 * BoundsSize;

    // a neighbour across an island edge jumps by whole faces; the vertex
    // normal is the safe answer there
    if(dot(pu, pu) > 400.0 || dot(pv, pv) > 400.0) {
        return n * normalFix;
    }

    float3 nd = cross(pu + hu * n, pv + hv * n);
    if(dot(nd, nd) > 1e-8) {
        if(dot(nd, n) < 0.0) {
            nd = -nd;
        }
        return normalize(nd) * normalFix;
    }
    return n * normalFix;
}
