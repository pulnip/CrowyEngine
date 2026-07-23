#include "Sampler.slang"
#include "SRGB.slang"

struct VertexIn {
    float2 position : POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

struct UIParam {
    DescriptorHandle<Texture2D> texture;
    // maps the screen-space vertex positions ImGui emits onto clip space
    float2 scale;
    float2 translate;
    // ImGui hands out sRGB-encoded colors, so a render target that encodes on
    // write needs them decoded first or the whole UI washes out
    uint srgbTarget;
};
[[vk::push_constant]]
ConstantBuffer<UIParam> param;

struct VertexOut{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

[shader("vertex")]
VertexOut vs_main(VertexIn input) {
    float2 pos = input.position * param.scale + param.translate;

    VertexOut output;
    output.position = float4(pos, 0, 1);
    output.uv = input.uv;
    output.color = input.color;

    if(param.srgbTarget != 0) {
        output.color.rgb = srgb2linear(output.color.rgb);
    }

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    Texture2D texture = param.texture;

    return input.color * texture.Sample(linearWrap, input.uv);
}
