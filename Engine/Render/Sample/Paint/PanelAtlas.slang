// The selected surface's atlas laid flat in a corner of the back buffer:
// its islands, or one channel of the paint buffer or the bake, texel for
// texel. The back buffer holds display values, so these are written as is.

#include "PaintCommon.slang"

// mirrored from PaintPanelPush in PaintLabGpu.hpp
struct PanelPush {
    DescriptorHandle<Texture2D> paint;
    DescriptorHandle<Texture2D> position;
    DescriptorHandle<Texture2D> edgeFade;
    uint channel;
    uint atlasSize;
    // the panel in back-buffer pixels: x, y, width, height
    float4 rect;
    // each island's texel rectangle: min x, min y, max x, max y; zero when off
    float4 islandRects[6];
};

[[vk::push_constant]]
ConstantBuffer<PanelPush> panel : register(b0);

struct PanelOut {
    float4 position : SV_Position;
};

[shader("vertex")]
PanelOut vs_panel(uint vertexID : SV_VertexID) {
    static const float2 corners[3] = {
        float2(-1.0, -1.0),
        float2(3.0, -1.0),
        float2(-1.0, 3.0)
    };

    PanelOut output;
    output.position = float4(corners[vertexID], 0.0, 1.0);

    return output;
}

[shader("fragment")]
float4 fs_panel(PanelOut input) : SV_Target {
    float2 uv = (input.position.xy - panel.rect.xy) / panel.rect.zw;
    int last = int(panel.atlasSize) - 1;
    int2 t = clamp(int2(floor(uv * float(panel.atlasSize))), int2(0, 0), int2(last, last));
    int3 texel = int3(t, 0);
    Texture2D paintTex = panel.paint;
    Texture2D positionTex = panel.position;
    Texture2D fadeTex = panel.edgeFade;
    float4 paint = paintTex.Load(texel);
    float4 position = positionTex.Load(texel);

    // the empty atlas reads as a dark weave
    float weave = fmod(floor(input.position.x / 6.0) + floor(input.position.y / 6.0), 2.0);
    float3 empty = weave > 0.5 ? float3(0.10, 0.10, 0.11) : float3(0.13, 0.13, 0.14);

    float3 display = empty;
    switch(panel.channel) {
    case PAINT_PANEL_ISLANDS: {
        for(uint d = 0; d < 6u; ++d) {
            float4 r = panel.islandRects[d];
            if(r.z > r.x && float(t.x) >= r.x && float(t.x) < r.z &&
               float(t.y) >= r.y && float(t.y) < r.w) {
                display = DirectionColor(d) * (position.w > 0.5 ? 1.0 : 0.45);
            }
        }
        break;
    }
    case PAINT_PANEL_PAINT_ID:
        display = position.w > 0.5 ? TeamDisplayColor(DecodePaintId(paint.r)) : empty;
        break;
    case PAINT_PANEL_HEIGHT:
        display = position.w > 0.5 ? RampColor(paint.g) : empty;
        break;
    case PAINT_PANEL_DISTANCE:
        display = position.w > 0.5 ? RampColor(paint.b) : empty;
        break;
    case PAINT_PANEL_POSITION:
        display = position.w > 0.5 ? saturate(position.xyz) : empty;
        break;
    case PAINT_PANEL_EDGE_FADE:
        display = position.w > 0.5 ? RampColor(fadeTex.Load(texel).r) : empty;
        break;
    }

    return float4(display, 1.0);
}
