// Lines where depth or the normal jumps between neighbouring pixels,
// multiplied into the scene colour so they keep the hue under them.
// params = (depthThreshold, normalThreshold, near, far)
#include "Fullscreen.slang"

// what a line multiplies the colour under it by
static const float LineDarkness = 0.35;

// the encoded normal back to a vector; a cleared texel decodes to zero
float3 decodeNormal(float4 texel) {
    return texel.xyz * 2.0 - 1.0;
}

// device depth is affine in 1 / z, so a jump relative to (a - z) is a jump
// relative to the view depth
float depthEdge(float z0, float z1, float a) {
    return abs(z1 - z0) / max(a - z0, 1e-6);
}

[shader("fragment")]
float4 fs_outline(FullscreenVertexOut input) : SV_Target {
    Texture2D depth = fullscreen.source;
    Texture2D normals = fullscreen.input0;
    uint width;
    uint height;
    depth.GetDimensions(width, height);

    // an out-of-range Load is undefined on Metal
    int2 p = int2(input.position.xy);
    int2 q = min(p + 1, int2(width, height) - 1);
    float a = fullscreen.params.w / (fullscreen.params.w - fullscreen.params.z);

    // Roberts cross: the two diagonals of the 2x2 block at p
    float d00 = depth.Load(int3(p, 0)).r;
    float d11 = depth.Load(int3(q, 0)).r;
    float d10 = depth.Load(int3(q.x, p.y, 0)).r;
    float d01 = depth.Load(int3(p.x, q.y, 0)).r;
    float dEdge = max(depthEdge(d00, d11, a), depthEdge(d10, d01, a));

    float3 n00 = decodeNormal(normals.Load(int3(p, 0)));
    float3 n11 = decodeNormal(normals.Load(int3(q, 0)));
    float3 n10 = decodeNormal(normals.Load(int3(q.x, p.y, 0)));
    float3 n01 = decodeNormal(normals.Load(int3(p.x, q.y, 0)));
    float nEdge = max(length(n11 - n00), length(n01 - n10));

    float edge = max(
        step(fullscreen.params.x, dEdge),
        step(fullscreen.params.y, nEdge)
    );

    return float4(lerp(1.0, LineDarkness, edge).xxx, 1.0);
}
