struct FS_Input{
    float4 position [[position]];
    float3 worldPosition;
    float3 normal;
    float2 uv;
};

struct FS_Input_Line{
    float4 position [[position]];
    float4 color;
};

struct FS_Input_Point{
    float4 position [[position]];
    float4 color;
    float  psize    [[point_size]];
};

struct Point{
    float3 position;
    float4 color;
};

struct Material{
    float4 baseColorFactor;
    float3 emissiveFactor;
    float metallic;
    float roughness;
    int flags;
};