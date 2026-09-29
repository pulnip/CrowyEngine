// VertexPullSpike's proof of a fullscreen pass that loads a target on a GPU:
// one that reads the depth target and blends nothing onto the scene colour
// must leave the picture Present copies out byte for byte.

#include "Fullscreen.slang"

[shader("fragment")]
float4 fs_blend(FullscreenVertexOut input) : SV_Target {
    // a real depth read, weighted by an alpha of 0 into every channel
    const float depth = LoadTexel(fullscreen.source, input.position).r;

    return float4(depth, depth, depth, 0.0);
}
