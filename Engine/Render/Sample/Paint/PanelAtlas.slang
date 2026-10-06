// the selected surface's atlas laid flat over the back buffer; it holds
// display values, so the channels are written as is

#include "PaintCommon.slang"
#include "PaintTriangle.slang"

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
    // the profile's line in atlas texels: from in xy, to in zw
    float4 profile;
};

[[vk::push_constant]]
ConstantBuffer<PanelPush> panel : register(b0);

[shader("fragment")]
float4 fs_panel(PaintTriangleOut input) : SV_Target {
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
    case PAINT_PANEL_PROFILE: {
        // G along the line, 0 at the bottom and full (9 cm) near the top
        float2 at = lerp(panel.profile.xy, panel.profile.zw, uv.x);
        int2 st = clamp(int2(floor(at)), int2(0, 0), int2(last, last));
        float g = paintTex.Load(int3(st, 0)).g;
        float y = (1.0 - uv.y - 0.08) / 0.84;
        float pixel = 1.0 / (panel.rect.w * 0.84);
        display = float3(0.10, 0.10, 0.11);
        // a line per 0.35, the default deposit, and the ceiling at 1
        if(abs(frac(y / 0.35 + 0.5) - 0.5) * 0.35 < pixel * 0.8)
            display = float3(0.24, 0.24, 0.26);
        if(abs(y - 1.0) < pixel * 1.5)
            display = float3(0.62, 0.89, 0.83);
        if(y >= 0.0 && y <= g)
            display = lerp(display, float3(0.12, 0.66, 0.56), 0.55);
        if(abs(y - g) < pixel * 2.0)
            display = float3(0.97, 0.97, 0.95);
        break;
    }
    }

    return float4(display, 1.0);
}
