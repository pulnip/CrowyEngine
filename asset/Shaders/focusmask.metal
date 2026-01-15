#include <metal_stdlib>

using namespace metal;

struct VertexOut{
    float4 position [[position]];
    float2 texCoord;
};

struct FocusParams{
    float2 focusCenter;
    float focusRadius;
    float falloff;
    float aspectRatio;
};

fragment float4 fs_focusmask(
    VertexOut in                 [[stage_in ]],
    constant FocusParams& params [[buffer(0)]]
){
    float2 diff = in.texCoord - params.focusCenter;
    // adjust for screen ratio
    diff.x *= params.aspectRatio;
    float dist = length(diff);

    // set  center of mask 1.0 for clearing
    // set outside of mask 0.0 for pixelize
    float mask = 1.0 - smoothstep(
        params.focusRadius,
        params.focusRadius + params.falloff,
        dist
    );

    return float4(mask, mask, mask, 1.0);
}
