// glTF metallic-roughness: GGX, height-correlated Smith and Schlick's
// Fresnel over a Lambert diffuse. A material's default shading model.
import Shading;

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

struct PBRShading : IShadingModel {
    static void accumulate(Surface s, LightSample light, float3 toViewer, inout LightSum sum) {
        float3 N = s.normal;
        float3 V = toViewer;
        float3 L = light.direction;

        float NoL = saturate(dot(N, L));
        float NoV = saturate(dot(N, V)) + 1e-5;

        float3 H = normalize(L + V);
        float NoH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));

        // every roughness under 0.032 renders alike
        float a = max(s.roughness * s.roughness, 1e-3);
        float3 f0 = lerp(float3(0.04, 0.04, 0.04), s.albedo, s.metallic);
        float3 F = fresnelSchlick(f0, VoH);

        float3 specular = F * distributionGGX(NoH, a) * visibilitySmith(NoV, NoL, a);
        float3 diffuse = (1.0 - F) * (1.0 - s.metallic) * s.albedo / PI;

        // one sum, not split into diffuse and specular: the terms keep the
        // order the pictures were recorded with
        sum.diffuse += (diffuse + specular) * light.radiance * NoL * light.attenuation;
    }

    static float3 ambient(Surface s, float3 toViewer, float3 irradiance) {
        return irradiance * s.albedo;
    }

    static float3 compose(Surface s, LightSum sum, float3 ambient) {
        return sum.diffuse + sum.specular + ambient + s.emissive;
    }
}

export struct MaterialShading : IShadingModel = PBRShading;
