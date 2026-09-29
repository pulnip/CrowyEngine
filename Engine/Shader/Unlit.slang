// The surface's own colour and its emission; the light loop compiles to
// nothing once this is linked.
import Shading;

struct UnlitShading : IShadingModel {
    static void accumulate(Surface s, LightSample light, float3 toViewer, inout LightSum sum) {}

    static float3 ambient(Surface s, float3 toViewer, float3 irradiance) {
        return float3(0.0, 0.0, 0.0);
    }

    static float3 compose(Surface s, LightSum sum, float3 ambient) {
        return s.albedo + s.emissive;
    }
}

export struct MaterialShading : IShadingModel = UnlitShading;
