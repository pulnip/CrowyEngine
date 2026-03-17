#include <metal_stdlib>

using namespace metal;

float hash(float2 p){
    return fract(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}

float noise2D(float2 p){
    float2 i = floor(p);
    float2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    return mix(
        mix(hash(i), hash(i + float2(1, 0)), f.x),
        mix(hash(i + float2(0, 1)), hash(i + float2(1, 1)), f.x),
        f.y
    );
}

float fbm(float2 p, int octaves){
    float v = 0.0;
    float a = 0.5;
    float2 shift = float2(100.0, 100.0);
    for(int i=0; i<octaves; ++i){
        v += a * noise2D(p);

        p = 2.0 * p + shift;
        a *= 0.5;
    }
    return v;
}

struct VertexOut{
    float4 position [[position]];
    float2 texCoord;
};

// https://tannerhelland.com/2012/09/18/convert-temperature-rgb-algorithm-code.html
float3 blackbodyRadiation(float tempKelvin){
    float t = tempKelvin / 100.0;

    float3 color;

    if(t <= 66){
        color.r = 1.0;
        color.g = 0.388557 * log(t) - 0.629373;
        if(t <= 19)
            color.b = 0.0;
        else
            color.b = 0.541085 * log(t-10) - 1.191581;
    }
    else{
        color.r = 1.287886 * pow(t-60, -0.133205);
        color.g = 1.125477 * pow(t-60, -0.075515);
        color.b = 1.0;
    }

    return saturate(color);
}

struct DiskGenParams{
    float rs;
    float diskInner;
    float diskOuter;
    float maxTempKelvin;
};

fragment float4 fs_disk_gen(
    VertexOut            input [[stage_in ]],
    constant DiskGenParams& dg [[buffer(0)]]
){
    // x for radius(0=inner, 1=outer),
    // y for theta (0=0    , 1=2pi  )
    float2 uv = input.texCoord;
    float r = mix(dg.diskInner, dg.diskOuter, uv.x);
    float theta = 2 * M_PI_F * uv.y;

    // Shakura–Sunyaev α-disk model
    float rRatio = dg.diskInner / r;
    float temp = dg.maxTempKelvin * pow(rRatio, 0.75) * pow(max(1.0 - sqrt(rRatio), 0.0), 0.25);

    // blackbody rgb
    float3 baseColor = blackbodyRadiation(temp);

    // brightness (Stefan-Boltzmann)
    float rPeak = (49.0 / 36.0) * dg.diskInner;
    float xPeak = dg.diskInner / rPeak;
    float peakTemp = dg.maxTempKelvin * pow(xPeak, 0.75) * pow(1.0 - sqrt(xPeak), 0.25);
    float intensity = pow(temp/peakTemp, 4);

    // turbulence
    float2 noiseCoord = float2(12.0 * uv.x, 3.0 * theta);
    float turb = fbm(noiseCoord, 4);
    float turbStrength = mix(0.4, 0.1, uv.x);
    intensity *= (1.0 - turbStrength + turb * turbStrength);

    float spiral = 0.5 * sin(2.0*theta - 15.0*uv.x) + 0.5;
    intensity *= mix(0.7, 1.0, spiral);

    float numBands = 5;
    float bands = smoothstep(0.1, 0.9, 0.5*sin(2*M_PI_F * numBands * uv.x) + 0.5);
    intensity *= mix(0.5, 1.0, bands);

    return float4(intensity * baseColor, 1);
}