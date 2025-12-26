#import <metal_stdlib>

using namespace metal;

struct PhongLight{
    float3 direction;
    float3 diffuse;
    float3 specular;
    float3 ambient;
};
inline float3 phongLighting(
    float3 baseColor, float3 normal,
    float3 worldPosition, float3 viewPosition,
    PhongLight light
){
    float3 diffuse = max(dot(normal, -light.direction), 0.0);
    float3 diffuseColor = light.diffuse * diffuse * baseColor;

    float3 viewDir = normalize(viewPosition - worldPosition);
    float3 reflectDir = reflect(light.direction, normal);
    float3 specular = pow(max(dot(viewDir, reflectDir), 0.0), 1);
    float3 specularColor = specular * light.specular;

    float3 ambientColor = light.ambient * diffuseColor;

    return diffuseColor + specularColor + ambientColor;
}

struct RimConstant{
    float3 rimBase;
    float rimPower;
    float rimStrength;
};
inline float3 rimLighting(
    float3 baseColor, float3 normal,
    float3 worldPosition, float3 viewPosition,
    constant RimConstant& rimc
){
    float3 toView = normalize(viewPosition - worldPosition);
    float rimFactor = smoothstep(0.0, 1.0, 1.0 - dot(toView, normal));
    float rim = pow(rimFactor, rimc.rimPower);
    float3 rimColor = rimc.rimStrength * rim * rimc.rimBase;
    return baseColor + rimColor;
}
