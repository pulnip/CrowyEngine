#pragma once

// Fullscreen.slang's vertex stage without its push, for the paint passes
// that bring their own at b0

struct PaintTriangleOut {
    float4 position : SV_Position;
};

// one triangle over the target; a scissor or the viewport keeps it in place
[shader("vertex")]
PaintTriangleOut vs_triangle(uint vertexID : SV_VertexID) {
    static const float2 corners[3] = {
        float2(-1.0, -1.0),
        float2(3.0, -1.0),
        float2(-1.0, 3.0)
    };

    PaintTriangleOut output;
    output.position = float4(corners[vertexID], 0.0, 1.0);

    return output;
}
