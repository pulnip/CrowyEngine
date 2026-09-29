// What a shading model sees. Imported, never included, so the pass and the
// module it links share one declaration of each type.

// one fragment's material, facing the viewer
struct Surface {
    float3 albedo;
    float metallic;
    float3 emissive;
    float roughness;
    // world, unit, toward the viewer: a double-sided back is flipped
    float3 normal;
    float opacity;
    // MaterialData's lanes; each model documents its reading
    float4 custom0;
    float4 custom1;
};

// one light as a shading model sees it
struct LightSample {
    // unit, surface to light
    float3 direction;
    // colour x intensity: before attenuation, before any cosine
    float3 radiance;
    // falloff x shadow, >= 0, never thresholded by the engine
    float attenuation;
    // the shadow term alone: 0 or 1 under the default hard filter
    float shadow;
};

// a model's running sum over the lights; the engine never reads it
struct LightSum {
    float3 diffuse;
    float3 specular;
};

// The engine walks the lights and calls accumulate once per light, then
// ambient and compose once per fragment.
interface IShadingModel {
    static void accumulate(Surface s, LightSample light, float3 toViewer, inout LightSum sum);
    // irradiance is the view's ambient at s.normal; a model may ignore it
    static float3 ambient(Surface s, float3 toViewer, float3 irradiance);
    // the fragment's scene radiance
    static float3 compose(Surface s, LightSum sum, float3 ambient);
}

// What a program that links no module resolves to: a depth-only pipeline
// runs none of it, and a colour pass that forgot its module draws magenta.
struct UnlinkedShading : IShadingModel {
    static void accumulate(Surface s, LightSample light, float3 toViewer, inout LightSum sum) {}
    static float3 ambient(Surface s, float3 toViewer, float3 irradiance) {
        return float3(0.0, 0.0, 0.0);
    }
    static float3 compose(Surface s, LightSum sum, float3 ambient) {
        return float3(1.0, 0.0, 1.0);
    }
}
