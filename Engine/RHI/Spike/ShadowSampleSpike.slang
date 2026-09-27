// Pass 1 writes a 256x128 depth map with no color attachment and no fragment
// stage. Pass 2 reads it back in two panes, left through Texture2D<float> and
// right through DepthTexture2D, at the texel a shadow receiver would load.

#include "ClipToTexture.slang"

// Depths that need every float32 mantissa bit, so a lower-precision read
// cannot pass; they alternate by frame, so a read of the other frame's
// depth shows.
static const float TRIANGLE_DEPTH[2] = {0.3, 0.35};
static const float BAR_DEPTH[2] = {0.7, 0.65};

struct SpikePush {
    // the same texture twice, once per type a receiver could declare
    DescriptorHandle<Texture2D<float>> asTexture;
    DescriptorHandle<DepthTexture2D> asDepthTexture;
    // the frame number's low bit: which depths pass 1 wrote
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

        // a bar over the right third
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
    // one triangle over the whole viewport
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

// red for the triangle, green for the bar, blue for the clear, yellow for the
// other frame's depth, magenta for anything else
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
        TextureUVToTexel(ClipToTextureUV(input.ndc), uint2(width, height));

    return classify(map.Load(int3(texel, 0)));
}

[shader("fragment")]
float4 fs_read_depth_texture(FullscreenOut input) : SV_Target {
    DepthTexture2D map = push.asDepthTexture;
    uint width;
    uint height;
    map.GetDimensions(width, height);
    const int2 texel =
        TextureUVToTexel(ClipToTextureUV(input.ndc), uint2(width, height));

    return classify(map.Load(int3(texel, 0)));
}
