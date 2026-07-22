// IEC 61966-2-1
float3 linear2srgb(float3 linear) {
    float3 lo = linear * 12.92;
    float3 hi = 1.055 * pow(linear, 1.0 / 2.4) - 0.055;
    return lerp(lo, hi, step(0.0031308, linear));
}
float3 srgb2linear(float3 srgb) {
    float3 lo = srgb / 12.92;
    float3 hi = pow((srgb + 0.055) / 1.055, 2.4);
    return lerp(lo, hi, step(0.04045, srgb));
}

float3 linear2srgbApprox(float3 linear) {
    return pow(linear, 1.0 / 2.2);
}
float3 srgb2linearApprox(float3 linear) {
    return pow(linear, 2.2);
}