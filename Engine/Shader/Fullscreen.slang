#pragma once

// What a fullscreen pass's shader includes: the push FramePipeline fills,
// mirrored from FullscreenPush in RenderSceneData.hpp, and the vertex stage
// the pass's pipeline takes from the including file. It declares no `view`:
// SceneData.slang's push sits at b0 too, so the two never meet in one file.

struct FullscreenPush{
    // the pass's reads in order; a depth target reads as .r
    DescriptorHandle<Texture2D> source;
    DescriptorHandle<Texture2D> input0;
    DescriptorHandle<Texture2D> input1;
    DescriptorHandle<Texture2D> input2;
    float4 params;
};

[[vk::push_constant]]
ConstantBuffer<FullscreenPush> fullscreen : register(b0);

struct FullscreenVertexOut{
    float4 position : SV_Position;
};

// one triangle over the whole viewport, so no diagonal splits the quads
// derivatives are taken over
[shader("vertex")]
FullscreenVertexOut vs_main(uint vertexID : SV_VertexID){
    static const float2 corners[3] = {
        float2(-1.0, -1.0),
        float2(3.0, -1.0),
        float2(-1.0, 3.0)
    };

    FullscreenVertexOut output;
    output.position = float4(corners[vertexID], 0.0, 1.0);

    return output;
}

// the texel under the pixel, one to one: no sampler, no flip
float4 LoadTexel(Texture2D texture, float4 position){
    return texture.Load(int3(int2(position.xy), 0));
}
