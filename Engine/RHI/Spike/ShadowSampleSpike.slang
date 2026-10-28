// Can a shader read back depth written with no color attachment or fragment
// stage, as Texture2D<float> (left pane) and as DepthTexture2D (right pane)?
// PASS: two identical panes, each a red right triangle in its top-left quarter
// (right angle top-left), a green bar over most of its right third, blue
// elsewhere.
// FAIL: magenta - not this frame's depth (a wrong-typed view, a lost bit);
// yellow - the other frame's depth, an edge let a write and a read overlap;
// black - a pane never drawn; all blue - pass 1 wrote nothing; the triangle
// bottom-left - the uv y convention differs; a stretched pane - x and y
// swapped; only the left pane wrong - only DepthTexture2D reads depth here.

#include "ClipToTexture.slang"

// Depths that need every float32 mantissa bit, so a lower-precision read
// cannot pass; they alternate by frame, so a stale read shows.
static const float TRIANGLE_DEPTH[2] = {0.3, 0.35};
static const float BAR_DEPTH[2] = {0.7, 0.65};

struct SpikePush {
    // the same texture twice, once per type a receiver could declare
    DescriptorHandle<Texture2D<float>> asTexture;
    DescriptorHandle<DepthTexture2D> asDepthTexture;
    uint parity;
    uint _pad0;
};

[[vk::push_constant]]
ConstantBuffer<SpikePush> push;

struct FullscreenOut {
    float4 position : SV_POSITION;
    float2 ndc : TEXCOORD0;
};

[shader("vertex")]
float4 vs_shapes(uint vertexID : SV_VertexID) : SV_POSITION {
    static const float2 positions[9] = {
        // a right triangle, right angle top-left, at texels (12, 6),
        // (115.5, 6), (12, 57.5): its hypotenuse misses every texel centre
        float2(-0.90625, 0.90625),
        float2(-0.09765625, 0.90625),
        float2(-0.90625, 0.1015625),

        float2(0.35, 0.9),
        float2(0.9, 0.9),
        float2(0.35, -0.9),
        float2(0.35, -0.9),
        float2(0.9, 0.9),
        float2(0.9, -0.9)
    };

    const float depth = vertexID < 3 ?
        TRIANGLE_DEPTH[push.parity] :
        BAR_DEPTH[push.parity];

    return float4(positions[vertexID], depth, 1.0);
}

[shader("vertex")]
FullscreenOut vs_fullscreen(uint vertexID : SV_VertexID) {
    static const float2 corners[3] = {
        float2(-1.0, -1.0),
        float2(3.0, -1.0),
        float2(-1.0, 3.0)
    };

    FullscreenOut output;
    output.position = float4(corners[vertexID], 0.0, 1.0);
    output.ndc = corners[vertexID];

    return output;
}

bool matches(float value, float expected) {
    return abs(value - expected) < 1e-6;
}

float4 classify(float depth) {
    const uint parity = push.parity;
    if(matches(depth, TRIANGLE_DEPTH[parity]))
        return float4(1.0, 0.0, 0.0, 1.0);
    if(matches(depth, BAR_DEPTH[parity]))
        return float4(0.0, 1.0, 0.0, 1.0);
    if(matches(depth, 1.0))
        return float4(0.0, 0.0, 1.0, 1.0);
    const bool stale = matches(depth, TRIANGLE_DEPTH[1 - parity]) ||
        matches(depth, BAR_DEPTH[1 - parity]);
    if(stale)
        return float4(1.0, 1.0, 0.0, 1.0);

    return float4(1.0, 0.0, 1.0, 1.0);
}

[shader("fragment")]
float4 fs_read_texture(FullscreenOut input) : SV_Target {
    Texture2D<float> map = push.asTexture;
    uint width;
    uint height;
    map.GetDimensions(width, height);
    const int2 texel =
        textureUVToTexel(clipToTextureUV(input.ndc), uint2(width, height));

    return classify(map.Load(int3(texel, 0)));
}

[shader("fragment")]
float4 fs_read_depth_texture(FullscreenOut input) : SV_Target {
    DepthTexture2D map = push.asDepthTexture;
    uint width;
    uint height;
    map.GetDimensions(width, height);
    const int2 texel =
        textureUVToTexel(clipToTextureUV(input.ndc), uint2(width, height));

    return classify(map.Load(int3(texel, 0)));
}
