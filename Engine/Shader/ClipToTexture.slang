#pragma once

// Clip-space xy, +y up, to the uv of a texture whose row 0 is on top.
// ShadowSampleSpike is the regression check for this contract.
float2 ClipToTextureUV(float2 clipXY){
    return float2(0.5 + 0.5 * clipXY.x, 0.5 - 0.5 * clipXY.y);
}

// the texel under uv, for uv in [0, 1)
int2 TextureUVToTexel(float2 uv, uint2 size){
    return int2(uv * float2(size));
}
