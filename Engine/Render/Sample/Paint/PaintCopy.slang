// the brush's rectangles taken back from the scratch target into the paint
// buffer texel for texel: TransitionAndCopyTexture as a scissored draw

#include "Fullscreen.slang"

[shader("fragment")]
float4 fs_copy(FullscreenVertexOut input) : SV_Target {
    return LoadTexel(fullscreen.source, input.position);
}
