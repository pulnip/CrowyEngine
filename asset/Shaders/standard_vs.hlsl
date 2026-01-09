#include "common.hlsli"

cbuffer Uniforms: register(b1){
    float4x4 mvp;
};

VertexOut vs_main(VertexIn input){
    VertexOut output;
    output.position = mul(mvp, float4(input.position, 1.0));
    output.normal = input.normal;
    output.texCoord = input.texCoord;
    return output;
}
