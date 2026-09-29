// VertexPullSpike's proof of the fullscreen kind on a GPU: a pass that reads
// the depth target and blends nothing onto the scene, then a copy to the back
// buffer, must leave its picture byte for byte.

#include "Fullscreen.slang"

[shader("fragment")]
float4 fs_blend(FullscreenVertexOut input) : SV_Target {
    // a real depth read, weighted by an alpha of 0 into every channel
    const float depth = LoadTexel(fullscreen.source, input.position).r;

    return float4(depth, depth, depth, 0.0);
}

[shader("fragment")]
float4 fs_copy(FullscreenVertexOut input) : SV_Target {
    return LoadTexel(fullscreen.source, input.position);
}
