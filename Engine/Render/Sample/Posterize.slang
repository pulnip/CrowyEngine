// Each channel snapped to params.x levels, on the display values the tone
// map wrote; the last entry, so the UI rides its pass.
#include "Fullscreen.slang"

[shader("fragment")]
float4 fs_main(FullscreenVertexOut input) : SV_Target {
    float4 color = LoadTexel(fullscreen.source, input.position);
    float steps = max(fullscreen.params.x, 2.0) - 1.0;

    return float4(floor(color.rgb * steps + 0.5) / steps, color.a);
}
