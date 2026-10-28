#pragma once

// a structured buffer packs it tightly
struct Vertex {
    float3 position;
    float3 normal;
    float2 texCoord;
    // w is the handedness
    float4 tangent;
};
