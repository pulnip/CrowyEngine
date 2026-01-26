struct VertexOut{
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
};

Texture2D albedoTex : register(t0);
Texture2D normalTex : register(t1);
SamplerState      s : register(s0);

float4 fs_cel_shading(VertexOut input) : SV_Target{
    float4 albedo = albedoTex.Sample(s, input.texCoord);
    // if background
    if(albedo.a < 0.01)
        return albedo;
    float3 normal = normalTex.Sample(s, input.texCoord).rgb * 2.0 - 1.0;

    // TODO. hard-coded light
    float3 lightDir = normalize(float3(0.5, 1.0, 0.5));
    float ndotl = max(dot(normal, lightDir), 0.0);

    // stepping
    float shadow = 0.3 +
                   0.4 * step(0.4, ndotl) +
                   0.4 * step(0.6, ndotl);

    float3 color = shadow * albedo.rgb;

    return float4(color, 1.0);

    // Rim Lighting
    // TODO. hard-coded view direction
    float3 viewDir = normalize(float3(0.0, 0.0, 1.0));
    float rim = 1.0 - max(dot(normal, viewDir), 0.0);
    rim = pow(rim, 3.0);

    float3 rimColor = float3(1.0, 1.0, 1.0);
    float rimStrength = 0.5;

    color += rimStrength * rim * rimColor;
}