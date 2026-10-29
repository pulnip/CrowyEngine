#include "Sampler.slang"
#include "SRGB.slang"

// keep in sync with ImDrawVert; col packs RGBA8 with R in the lowest byte
struct ImDrawVert {
    float2 pos;
    float2 uv;
    uint col;
};

struct UIParam {
    DescriptorHandle<Texture2D> texture;
    DescriptorHandle<StructuredBuffer<ImDrawVert>> vertices;
    // maps the screen-space vertex positions ImGui emits onto clip space
    float2 scale;
    float2 translate;
    // ImGui hands out sRGB-encoded colors, so a render target that encodes on
    // write needs them decoded first or the whole UI washes out
    uint srgbTarget;
    // SV_VertexID leaves out baseVertex on D3D12 and folds it in on Metal, so
    // draws pass baseVertex 0 and the list's first row travels here
    uint vertexBase;
};
[[vk::push_constant]]
ConstantBuffer<UIParam> param;

struct VertexOut{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

[shader("vertex")]
VertexOut vs_main(uint vertexID : SV_VertexID) {
    StructuredBuffer<ImDrawVert> vertices = param.vertices;
    ImDrawVert input = vertices[param.vertexBase + vertexID];

    float2 pos = input.pos * param.scale + param.translate;

    VertexOut output;
    output.position = float4(pos, 0, 1);
    output.uv = input.uv;
    output.color = unpackUnorm4x8ToFloat(input.col);

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
