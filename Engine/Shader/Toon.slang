// Cel shading after Godot's DIFFUSE_TOON: a terminator band as wide as the
// roughness, a hard highlight disc and a rim, over a flat shade tint.
//
// custom0 = (shadowThreshold, highlightSize, rimStrength, rimWidth)
// custom1 = (shadeTint.rgb, unused); ToonLanes in ShadingModel.hpp mirrors it
import Shading;

static const float PI = 3.14159265;

struct ToonShading : IShadingModel {
    static void accumulate(Surface s, LightSample light, float3 toViewer, inout LightSum sum) {
        // signed N.L: the band straddles the terminator
        float NoL = dot(s.normal, light.direction);
        float w = max(s.roughness, 0.01);

        // Inside the band the shadow map's own terminator would land mid-ramp
        // as texel stairs, so there the band alone shades. A light in full
        // shadow is the directional one, whose falloff is 1.
        float unshadowed = light.shadow > 0.0 ? light.attenuation / light.shadow : 1.0;
        // the engine never thresholds attenuation; this model does
        float lit = step(s.custom0.x, NoL < w ? unshadowed : light.attenuation);

        float band = smoothstep(-w, w, NoL) * lit;
        sum.diffuse += light.radiance * band / PI;

        // a disc where the mirrored light meets the eye
        float3 R = reflect(-light.direction, s.normal);
        float highlight = step(1.0 - s.custom0.y, dot(R, toViewer)) * band;
        // step(1, x) is 1 at x = 1 exactly: a zero size turns the disc off
        highlight *= step(1e-6, s.custom0.y);

        // the silhouette's edge, as wide as rimWidth; unshadowed, as Godot's,
        // so it also rims the dark side where a bounce light sits
        float edge = 1.0 - saturate(dot(s.normal, toViewer));
        float rim = smoothstep(1.0 - max(s.custom0.w, 1e-3), 1.0, edge) * s.custom0.z;

        sum.specular += light.radiance * (highlight + rim);
    }

    // the unlit side is the material's tint; the sky is ignored
    static float3 ambient(Surface s, float3 toViewer, float3 irradiance) {
        return s.custom1.rgb;
    }

    static float3 compose(Surface s, LightSum sum, float3 ambient) {
        return s.albedo * (ambient + sum.diffuse) + sum.specular + s.emissive;
    }
}

export struct MaterialShading : IShadingModel = ToonShading;
