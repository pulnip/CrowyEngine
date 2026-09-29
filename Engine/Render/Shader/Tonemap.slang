// The linear-to-display boundary: the post list's first entry, and every
// entry after it works in display values.

#include "Fullscreen.slang"
#include "SRGB.slang"

// Reinhard, then encoded by hand: the swapchain is UNORM, not sRGB
float3 toDisplay(float3 color) {
    return linear2srgb(color / (1.0 + color));
}

[shader("fragment")]
float4 fs_main(FullscreenVertexOut input) : SV_Target {
    float4 radiance = LoadTexel(fullscreen.source, input.position);

    // alpha passes through, so a blended surface keeps its coverage
    return float4(toDisplay(radiance.rgb), radiance.a);
}
