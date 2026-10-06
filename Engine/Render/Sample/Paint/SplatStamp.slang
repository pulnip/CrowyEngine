#pragma once

// MintChoco's splat stamp: MF_SplatStampMask's Custom node, verbatim but for
// syntax, and the shape slider's build-up from a circle to it.
// uv = (U / (R S), V / R) and n = N / R, from the stamp's basis; every
// function returns (mask, kernel, d), d the stamp-space signed distance to the
// splat edge, negative inside.

float3 StampCustom(float2 uv, float n, float Seed, float ImpactU, float Shape) {
    const float MainRadius = 0.5;
    const float SpikePower = 1.5;
    const float Throw = 0.55;
    const float Tear = 2.2;
    const float SatBaseSize = 0.13;
    const float BlendK = 0.07;
    const float CrinkleFreq = 6.0;
    const float CrinkleAmp = 0.09;
    const int MaxSat = 16;
    // Shape scales every irregularity - rim wobble, droplets, crinkle,
    // mottled deposit - so 0 leaves a clean ellipse.
    float shape = saturate(Shape);

    if(abs(n) >= 1.0) {
        return float3(0.0, 0.0, 1e3);
    }
    float2 p = uv / sqrt(max(1.0 - n * n, 1e-4));

    // Main blob: a disc whose radius wobbles with seeded low harmonics -
    // never a perfect circle.
    float sA = frac(sin(Seed * 0.317) * 43758.5453);
    float sB = frac(sin(Seed * 0.771) * 24634.6294);
    float sC = frac(sin(Seed * 1.137) * 33758.5453);
    float sP1 = frac(sin(Seed * 1.531) * 18273.1837);
    float sP2 = frac(sin(Seed * 2.117) * 43758.5453);
    float sP3 = frac(sin(Seed * 2.713) * 24634.6294);
    float theta = atan2(p.y, p.x);
    float wobble = 1.0 +
        shape * (0.05 + 0.08 * sA) * sin(2.0 * theta + sP1 * 6.2831853) +
        shape * (0.04 + 0.07 * sB) * sin(3.0 * theta + sP2 * 6.2831853) +
        shape * (0.02 + 0.05 * sC) * sin(5.0 * theta + sP3 * 6.2831853);
    float d = length(p) - MainRadius * wobble;

    float2 impact = float2(ImpactU, 0.0);
    float maxImpactDist = MainRadius + abs(ImpactU);

    // Satellites: the seed also decides how many, where they cluster, and
    // which ones tear.
    float density =
        shape * (0.35 + 0.55 * frac(sin(Seed * 0.531) * 24634.6294));
    for(int i = 0; i < MaxSat; i++) {
        float fi = (float)i;
        float hA = frac(sin(Seed + fi * 12.9898) * 43758.5453);
        float hB = frac(sin(Seed * 1.7 + fi * 78.2330) * 24634.6294);
        float hC = frac(sin(Seed * 2.3 + fi * 37.7190) * 33758.5453);
        float hD = frac(sin(Seed * 3.1 + fi * 53.7170) * 18273.1837);
        float hE = frac(sin(Seed * 4.3 + fi * 91.3570) * 43758.5453);
        if(hA > density) {
            continue;
        }

        float ang = hB * 6.2831853;
        float2 dir = float2(cos(ang), sin(ang));

        // The spike field: weak near the impact point, strong at the far
        // rim, seed-jittered.
        float spike = saturate(length(dir * MainRadius - impact) / maxImpactDist);
        spike = pow(spike, SpikePower) * (0.25 + 0.75 * hC);

        // Quadratic bias: most droplets hug the rim, a few fly far; big
        // lobes are rare.
        float t = hD * hD;
        float dist = MainRadius * (0.7 + 0.9 * t) + spike * Throw;
        float tear = 1.0 + Tear * spike * hC;
        float size = SatBaseSize * (0.35 + 1.3 * hE * hE) * (1.0 - 0.3 * spike);
        dist = min(dist, 0.95 - size * tear);

        float2 c = dir * dist;
        float2 dd = p - c;
        float along = dot(dd, dir);
        float2 perp = dd - along * dir;
        float dsat = sqrt(along * along / (tear * tear) + dot(perp, perp)) - size;

        // Smooth-min union: near droplets melt into the blob with a liquid
        // neck.
        float hh = saturate(0.5 + 0.5 * (dsat - d) / BlendK);
        d = lerp(dsat, d, hh) - BlendK * hh * (1.0 - hh);
    }

    // Edge crinkle: two octaves of value noise wobble every boundary like
    // real ink.
    float2 nq = p * CrinkleFreq +
        float2(frac(sin(Seed * 0.777) * 43758.5453) * 37.0,
               frac(sin(Seed * 0.913) * 24634.6294) * 57.0);
    float2 ip1 = floor(nq);
    float2 f1 = frac(nq);
    f1 = f1 * f1 * (3.0 - 2.0 * f1);
    float n00 = frac(sin(dot(ip1, float2(127.1, 311.7))) * 43758.5453);
    float n10 = frac(sin(dot(ip1 + float2(1.0, 0.0), float2(127.1, 311.7))) * 43758.5453);
    float n01 = frac(sin(dot(ip1 + float2(0.0, 1.0), float2(127.1, 311.7))) * 43758.5453);
    float n11 = frac(sin(dot(ip1 + float2(1.0, 1.0), float2(127.1, 311.7))) * 43758.5453);
    float cr1 = lerp(lerp(n00, n10, f1.x), lerp(n01, n11, f1.x), f1.y);
    float2 nq2 = nq * 2.7 + 13.1;
    float2 ip2 = floor(nq2);
    float2 f2 = frac(nq2);
    f2 = f2 * f2 * (3.0 - 2.0 * f2);
    float m00 = frac(sin(dot(ip2, float2(127.1, 311.7))) * 43758.5453);
    float m10 = frac(sin(dot(ip2 + float2(1.0, 0.0), float2(127.1, 311.7))) * 43758.5453);
    float m01 = frac(sin(dot(ip2 + float2(0.0, 1.0), float2(127.1, 311.7))) * 43758.5453);
    float m11 = frac(sin(dot(ip2 + float2(1.0, 1.0), float2(127.1, 311.7))) * 43758.5453);
    float cr2 = lerp(lerp(m00, m10, f2.x), lerp(m01, m11, f2.x), f2.y);
    d += shape * ((cr1 - 0.5) * CrinkleAmp + (cr2 - 0.5) * CrinkleAmp * 0.45);

    float mask = (d < 0.0) ? 1.0 : 0.0;
    // Soft deposit kernel: full over the blob's core, feathering across the
    // outer quarter-radius, mottled by the same seeded noise so strokes pile
    // up unevenly.
    float k = smoothstep(0.0, 1.0, saturate(-d / 0.25));
    k *= lerp(1.0, 0.8 + 0.4 * cr1, shape);

    return float3(mask, saturate(k), d);
}

// the hash StampCustom draws everything from
float stampHash(float x, float k) {
    return frac(sin(x) * k);
}

// value noise as StampCustom's crinkle octaves read it
float stampValueNoise(float2 q) {
    float2 i = floor(q);
    float2 f = frac(q);
    f = f * f * (3.0 - 2.0 * f);
    float a = frac(sin(dot(i, float2(127.1, 311.7))) * 43758.5453);
    float b = frac(sin(dot(i + float2(1.0, 0.0), float2(127.1, 311.7))) * 43758.5453);
    float c = frac(sin(dot(i + float2(0.0, 1.0), float2(127.1, 311.7))) * 43758.5453);
    float e = frac(sin(dot(i + float2(1.0, 1.0), float2(127.1, 311.7))) * 43758.5453);

    return lerp(lerp(a, b, f.x), lerp(c, e, f.x), f.y);
}

// One whole stop of the build-up, stage 0 to 7; stage 8 is StampCustom with
// Shape 1, whose d stage 7 already equals. `roundUV` is (U / R, V / R): the
// circle at stage 0 is round in the world, before the stretch exists.
float stampStageDistance(float2 uv, float2 roundUV, float n, float Seed, float ImpactU, int stage) {
    const float MainRadius = 0.5;

    if(stage == 0)
        return length(roundUV) - MainRadius;

    float2 p = uv / sqrt(max(1.0 - n * n, 1e-4));
    float theta = atan2(p.y, p.x);
    float wobble = 1.0;
    if(stage >= 2) {
        wobble += (0.05 + 0.08 * stampHash(Seed * 0.317, 43758.5453)) *
                      sin(2.0 * theta + stampHash(Seed * 1.531, 18273.1837) * 6.2831853) +
                  (0.04 + 0.07 * stampHash(Seed * 0.771, 24634.6294)) *
                      sin(3.0 * theta + stampHash(Seed * 2.117, 43758.5453) * 6.2831853) +
                  (0.02 + 0.05 * stampHash(Seed * 1.137, 33758.5453)) *
                      sin(5.0 * theta + stampHash(Seed * 2.713, 24634.6294) * 6.2831853);
    }
    float d = length(p) - MainRadius * wobble;
    if(stage < 3)
        return d;

    if(stage == 3) {
        // the sunflower: petals at even angles, every one of them, each a
        // little longer or bigger than the next, joined with a hard min
        for(int i = 0; i < 10; i++) {
            float fi = (float)i;
            float hD = stampHash(Seed * 3.1 + fi * 53.7170, 18273.1837);
            float hE = stampHash(Seed * 4.3 + fi * 91.3570, 43758.5453);
            float ang = fi * 0.62831853;
            float2 dir = float2(cos(ang), sin(ang));
            float tear = 2.2;
            float size = 0.13 * (0.6 + 0.6 * hE);
            float dist = min(MainRadius * (0.85 + 0.45 * hD), 0.95 - size * tear);
            float2 dd = p - dir * dist;
            float along = dot(dd, dir);
            float2 perp = dd - along * dir;
            d = min(d, sqrt(along * along / (tear * tear) + dot(perp, perp)) - size);
        }

        return d;
    }

    // stages 4 to 7: StampCustom's own satellites, its terms added in order
    const float SpikePower = 1.5;
    const float Throw = 0.55;
    const float Tear = 2.2;
    const float SatBaseSize = 0.13;
    const float BlendK = 0.07;
    float2 impact = float2(ImpactU, 0.0);
    float maxImpactDist = MainRadius + abs(ImpactU);
    float density = 0.35 + 0.55 * stampHash(Seed * 0.531, 24634.6294);
    for(int i = 0; i < 16; i++) {
        float fi = (float)i;
        float hA = stampHash(Seed + fi * 12.9898, 43758.5453);
        float hB = stampHash(Seed * 1.7 + fi * 78.2330, 24634.6294);
        float hC = stampHash(Seed * 2.3 + fi * 37.7190, 33758.5453);
        float hD = stampHash(Seed * 3.1 + fi * 53.7170, 18273.1837);
        float hE = stampHash(Seed * 4.3 + fi * 91.3570, 43758.5453);
        if(hA > density)
            continue;

        float ang = hB * 6.2831853;
        float2 dir = float2(cos(ang), sin(ang));
        float spike = 0.0;
        if(stage >= 5) {
            spike = saturate(length(dir * MainRadius - impact) / maxImpactDist);
            spike = pow(spike, SpikePower) * (0.25 + 0.75 * hC);
        }
        float t = hD * hD;
        float dist = MainRadius * (0.7 + 0.9 * t) + spike * Throw;
        float tear = 1.0 + Tear * spike * hC;
        float size = SatBaseSize * (0.35 + 1.3 * hE * hE) * (1.0 - 0.3 * spike);
        dist = min(dist, 0.95 - size * tear);

        float2 dd = p - dir * dist;
        float along = dot(dd, dir);
        float2 perp = dd - along * dir;
        float dsat = sqrt(along * along / (tear * tear) + dot(perp, perp)) - size;
        if(stage >= 6) {
            float hh = saturate(0.5 + 0.5 * (dsat - d) / BlendK);
            d = lerp(dsat, d, hh) - BlendK * hh * (1.0 - hh);
        } else {
            d = min(d, dsat);
        }
    }

    if(stage >= 7) {
        const float CrinkleFreq = 6.0;
        const float CrinkleAmp = 0.09;
        float2 nq = p * CrinkleFreq +
            float2(stampHash(Seed * 0.777, 43758.5453) * 37.0,
                   stampHash(Seed * 0.913, 24634.6294) * 57.0);
        float cr1 = stampValueNoise(nq);
        float cr2 = stampValueNoise(nq * 2.7 + 13.1);
        d += (cr1 - 0.5) * CrinkleAmp + (cr2 - 0.5) * CrinkleAmp * 0.45;
    }

    return d;
}

// one stop of the slider; Shape fades each irregularity back to stage 1's
// clean ellipse, as BrushShapeNoise does to StampCustom
float stampStagedStop(float2 uv, float2 roundUV, float n, float Seed, float ImpactU, float Shape, int stage) {
    if(stage >= 8)
        return StampCustom(uv, n, Seed, ImpactU, Shape).z;
    float d = stampStageDistance(uv, roundUV, n, Seed, ImpactU, stage);
    if(stage < 2)
        return d;
    return lerp(stampStageDistance(uv, roundUV, n, Seed, ImpactU, 1), d, saturate(Shape));
}

// the shape slider: stage 0 a circle, 8 the real stamp; between two stops
// the distance blends linearly. Below 8 the kernel carries no mottling.
float3 StampStaged(float2 uv, float stretch, float n, float Seed, float ImpactU, float Shape, float stage) {
    if(abs(n) >= 1.0)
        return float3(0.0, 0.0, 1e3);
    if(stage >= PAINT_SHAPE_STAGE_FULL)
        return StampCustom(uv, n, Seed, ImpactU, Shape);

    float s = clamp(stage, 0.0, PAINT_SHAPE_STAGE_FULL);
    int lo = (int)floor(s);
    float blend = s - (float)lo;
    float2 roundUV = float2(uv.x * stretch, uv.y);
    float d = stampStagedStop(uv, roundUV, n, Seed, ImpactU, Shape, lo);
    if(blend > 0.0)
        d = lerp(d, stampStagedStop(uv, roundUV, n, Seed, ImpactU, Shape, lo + 1), blend);

    float mask = (d < 0.0) ? 1.0 : 0.0;
    float k = smoothstep(0.0, 1.0, saturate(-d / 0.25));

    return float3(mask, saturate(k), d);
}
