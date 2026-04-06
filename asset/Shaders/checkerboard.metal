#include <metal_stdlib>

using namespace metal;

kernel void cs_checkerboard(
    texture2d<float, access::write> out [[texture(0)]],
    uint2              tid [[thread_position_in_grid]]
){
    if(tid.x >= out.get_width() || tid.y >= out.get_height()) return;

    // 8x8
    bool isWhite = ((tid.x / 8) + (tid.y / 8)) % 2 == 0;

    constexpr float4 WHITE = float4(1.0, 1.0, 1.0, 1.0);
    constexpr float4 BLACK = float4(0.0, 0.0, 0.0, 1.0);
    out.write(isWhite ? WHITE : BLACK, tid);
}