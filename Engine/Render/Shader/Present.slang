// An exact copy of the source: the last entry for a scene already in
// display values, and for the data views, which no tone map may distort.

#include "Fullscreen.slang"

[shader("fragment")]
float4 fs_main(FullscreenVertexOut input) : SV_Target {
    return LoadTexel(fullscreen.source, input.position);
}
