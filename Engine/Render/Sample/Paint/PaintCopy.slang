// The brush's rectangles taken back from the scratch target into the paint
// buffer, texel for texel: TransitionAndCopyTexture, as a scissored draw.

struct CopyPush {
    DescriptorHandle<Texture2D> source;
};

[[vk::push_constant]]
ConstantBuffer<CopyPush> copyPush : register(b0);

struct CopyOut {
    float4 position : SV_Position;
};

[shader("vertex")]
CopyOut vs_copy(uint vertexID : SV_VertexID) {
    static const float2 corners[3] = {
        float2(-1.0, -1.0),
        float2(3.0, -1.0),
        float2(-1.0, 3.0)
    };

    CopyOut output;
    output.position = float4(corners[vertexID], 0.0, 1.0);

    return output;
}

[shader("fragment")]
float4 fs_copy(CopyOut input) : SV_Target {
    Texture2D source = copyPush.source;

    return source.Load(int3(int2(input.position.xy), 0));
}
