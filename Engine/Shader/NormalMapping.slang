struct VertexIn {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
    // xyz = tangent direction, w = handedness sign
    float4 tangent : TANGENT;
};

cbuffer uniform : register(b1) {
    // rigid rotation, so it transforms normals and tangents unchanged
    float4x4 model;
    float4x4 viewProj;
    float3 cameraPos;
    float parallaxScale;
    float3 toLight;
    float specularPower;
    float ambient;
};

struct VertexOut {
    float4 posProj : SV_Position;
    float3 posWorld : POSITION;
    float3 normal : NORMAL;
    // w carries the handedness sign through unchanged
    float4 tangent : TANGENT;
    float2 texCoord : TEXCOORD0;
};

[shader("vertex")]
VertexOut vs_main(VertexIn input) {
    float4 posWorld = mul(model, float4(input.position, 1.0));

    VertexOut output;
    output.posProj = mul(viewProj, posWorld);
    output.posWorld = posWorld.xyz;
    output.normal = mul((float3x3)model, input.normal);
    output.tangent = float4(mul((float3x3)model, input.tangent.xyz), input.tangent.w);
    output.texCoord = input.texCoord;

    return output;
}

struct ResourceData {
    DescriptorHandle<Texture2D> diffuse;
    // BC5_UNORM. only RG is stored, Z is reconstructed.
    // this is the shading
    // detail: smooth wide bevels along each plank
    DescriptorHandle<Texture2D> normal;
    // BC4_UNORM. height in R.
    // sharper than the normal map and it carries the frame and the diagonal brace,
    // so it drives the parallax offset
    DescriptorHandle<Texture2D> bump;
};

[[vk::push_constant]]
ConstantBuffer<ResourceData> resource : register(b0);
[[vk::binding(1, 0)]]
SamplerState linearWrap : register(s0);

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    Texture2D diffuseTex = resource.diffuse;
    Texture2D normalTex = resource.normal;
    Texture2D bumpTex = resource.bump;

    // interpolation leaves the frame slightly non-orthogonal, so re-orthogonalize.
    // bitangent runs along +v, which points down in texture space
    float3 N = normalize(input.normal);
    float3 T = normalize(input.tangent.xyz - N * dot(N, input.tangent.xyz));
    float3 B = input.tangent.w * cross(N, T);
    // mul(tbn, v): world -> tangent,
    // mul(v, tbn): tangent -> world
    float3x3 tbn = float3x3(T, B, N);

    float3 viewDirW = normalize(cameraPos.xyz - input.posWorld);

    // parallax with offset limiting. shifts the uv along the tangent-space view
    // direction so the height field looks displaced
    float3 viewDirT = mul(tbn, viewDirW);
    float height = bumpTex.Sample(linearWrap, input.texCoord).r;
    float2 shift = parallaxScale * (height - 0.5) * viewDirT.xy / max(viewDirT.z, 0.1);
    float2 uv = input.texCoord + shift;

    // green is stored as -dH/dv, the v-down convention,
    // which is the frame bitangent already runs along.
    float2 nxy = normalTex.Sample(linearWrap, uv).rg * 2.0 - 1.0;
    float nz = sqrt(saturate(1.0 - dot(nxy, nxy)));
    float3 n = normalize(mul(float3(nxy, nz), tbn));

    float3 albedo = diffuseTex.Sample(linearWrap, uv).rgb;

    float3 L = normalize(toLight.xyz);
    float3 H = normalize(L + viewDirW);

    float ndotl = saturate(dot(n, L));
    // no specular on surfaces facing away from the light
    float spec = ndotl > 0.0 ? pow(saturate(dot(n, H)), specularPower) : 0.0;

    float3 color = albedo * (ambient + ndotl) + spec * 0.3;

    return float4(color, 1.0);
}
