#import "common.metal"
#import "lighting.metal"

constant float3 lightDirection = float3(0.436436, -0.872872, 0.218218);

constant float3 lightDiffuse = float3(1.0);
constant float3 lightSpecular = float3(0.5);
constant float3 lightAmbient = float3(0.2);

fragment float4 fragment_main(
    FS_Input input                 [[stage_in]],
    constant float3& viewPosition  [[buffer(0)]],
    constant float4& myIDColor     [[buffer(1)]],
    constant float4& pickedIDColor [[buffer(2)]],
    constant float4& debugColor    [[buffer(3)]],
    constant float& debugAlpha     [[buffer(4)]],
    texture2d<float> baseColor     [[texture(0)]],
    // texture2d<float> texNormal     [[texture(1)]],
    // texture2d<float> texMR         [[texture(2)]],
    // texture2d<float> texEmissive   [[texture(3)]],
    sampler samp                   [[sampler(0)]]
){
    float2 uv = input.uv.xy;
    float4 color = debugColor;
    if(color.x!=-1)
        color = baseColor.sample(samp, uv);

    PhongLight light{
        lightDirection,
        lightDiffuse,
        lightSpecular,
        lightAmbient
    };

    float3 phongColor = phongLighting(color.rgb, input.normal,
        input.worldPosition, viewPosition, light);
    // float3 rimColor = rimLighting(color.rgb, input.normal,
    //     input.worldPosition, viewPosition, rimc);
    float3 lightingColor = phongColor; // + rimColor;

    float3 red = float3(1.0, 0.0, 0.0);
    float m = 0.0;
    if((myIDColor == pickedIDColor).x)
        m = 0.5;
    float3 mixed = mix(lightingColor, red, m);

    // float3 white = float3(0.0, 0.0, 0.0);
    return float4(mixed, color.a);
    // return float4(mix(white, mixed, debugAlpha), color.a);
}

fragment float4 fragment_id(
    FS_Input input             [[stage_in]],
    constant float4 &myIDColor [[buffer(1)]]
){
    return myIDColor;
}

fragment float4 fragment_line(
    FS_Input_Line input        [[stage_in]]
){
    return float4(input.color.rgb, 1.0);
}

fragment float4 fragment_points(
    FS_Input_Point input       [[stage_in]]
){
    return float4(input.color.rgb, 1.0);
}
