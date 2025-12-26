#import <metal_stdlib>
#import "common.metal"

using namespace metal;

struct Vertex{
    float3 position             [[attribute(0)]];
    float3 normal               [[attribute(1)]];
    float2 uv                   [[attribute(2)]];
    float3 tangent              [[attribute(3)]];
};
struct Line{
    float3 from                 [[attribute(0)]];
    float3 to                   [[attribute(1)]];
    float4 color                [[attribute(2)]];
};

struct ViewConstant{
    float4x4 trs;
    float4x4 proj;
};
struct ModelConstant{
    float4x4 trs;
    float3x3 normal;
};

vertex FS_Input vertex_main(uint vid [[vertex_id]],
    constant Vertex* vertices     [[buffer(0)]],
    constant ViewConstant& view   [[buffer(3)]],
    constant ModelConstant& model [[buffer(4)]]
){
    FS_Input output;
    output.worldPosition = (model.trs*float4(vertices[vid].position, 1.0)).xyz;
    output.position = view.proj * view.trs * float4(output.worldPosition, 1.0);
    output.normal = model.normal * vertices[vid].normal.xyz;
    output.uv = vertices[vid].uv.xy;
    return output;
}

vertex FS_Input_Line vertex_line(uint vid [[vertex_id]],
    constant Line* lines          [[buffer(0)]],
    constant ViewConstant& view   [[buffer(1)]]
){
    FS_Input_Line output;

    uint idx = vid / 2;
    uint sub = vid % 2;
    float3 pos = (sub==0) ? lines[idx].from: lines[idx].to;

    output.position = view.proj * view.trs * float4(pos, 1.0);
    output.color = lines[idx].color;

    return output;
}

vertex FS_Input_Point vertex_points(uint vid [[vertex_id]],
    device const Point* pts       [[buffer(0)]],
    constant float4x4& mvp        [[buffer(1)]]
){
    FS_Input_Point output;

    Point point = pts[vid];
    output.position = mvp * float4(point.position, 1.0);
    output.color = point.color;
    output.psize = 1.0;
    return output;
}
