#pragma once

// The microfacet terms of the metallic-roughness BRDF, apart from any
// shading model, so a sample with its own light loop shares them.

static const float PI = 3.14159265;

float3 fresnelSchlick(float3 f0, float cosTheta) {
    return f0 + (1.0 - f0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float distributionGGX(float NoH, float a) {
    float a2 = a * a;
    float d = NoH * NoH * (a2 - 1.0) + 1.0;

    return a2 / max(PI * d * d, 1e-6);
}

// Smith height-correlated, already carrying the 1/(4*NoL*NoV) of the BRDF
float visibilitySmith(float NoV, float NoL, float a) {
    float a2 = a * a;
    float v = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float l = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);

    return 0.5 / max(v + l, 1e-6);
}
