#include <metal_stdlib>

using namespace metal;

// [0, 1] to [-1, 1]
float2 uv2ndc(float2 uv){
    return float2(2.0 * uv.x - 1.0, 1.0 - 2.0 * uv.y);
}

// [-1, 1] to Image Plane
float2 ndc2ip(float2 ndc, float aspect, float tanHalfFov){
    return ndc * float2(aspect, 1.0) * tanHalfFov;
}

float3 ip2RayDir(float2 ip, float3 right, float3 up, float3 forward){
    return normalize(ip.x * right + ip.y * up + forward);
}

struct VertexOut{
    float4 position [[position]];
    float2 texCoord;
};

struct BlackholeParams{
    packed_float3 bhPos;
    float rs;
    packed_float3 camPos;
    float aspect;
    packed_float3 camRight;
    float tanHalfFov;
    packed_float3 camUp;
    float diskInner;
    packed_float3 camForward;
    float diskOuter;
};

float3 sphericalToCartesian(float r, float theta, float phi){
    float sint = sin(theta), cost = cos(theta);
    float sinp = sin(phi  ), cosp = cos(phi  );

    return float3(
        r * sint*cosp,
        r * cost,
        r * sint*sinp
    );
}

struct GeoState{
    float3 pos;
    float3 vel;
};

GeoState initGeoState(float3 pos, float3 dir, float rs){
    GeoState s;
    s.pos = pos;
    s.vel = dir;

    return s;
}

struct GeoDerives{
    float3 dpos;
    float3 dvel;
};

GeoDerives geodesicRHS(GeoState s, float rs){
    float r = length(s.pos);
    float3 nhat = s.pos / r;
    float vr = dot(s.vel, nhat);
    float vperp_sq = dot(s.vel, s.vel) - vr*vr;

    GeoDerives d;
    d.dpos = s.vel;
    d.dvel = -1.5*rs / (r*r) * vperp_sq*nhat;

    return d;
}

GeoState applyDerives(GeoState s, GeoDerives d, float h){
    GeoState n;
    n.pos = s.pos + d.dpos * h;
    n.vel = s.vel + d.dvel * h;

    return n;
}

void rk4(thread GeoState& s, float dl, float rs){
    GeoDerives k1 = geodesicRHS(                          s, rs);
    GeoDerives k2 = geodesicRHS(applyDerives(s, k1, 0.5*dl), rs);
    GeoDerives k3 = geodesicRHS(applyDerives(s, k2, 0.5*dl), rs);
    GeoDerives k4 = geodesicRHS(applyDerives(s, k3,     dl), rs);

    s.pos += (dl/6.0) * (k1.dpos + 2*k2.dpos + 2*k3.dpos + k4.dpos);
    s.vel += (dl/6.0) * (k1.dvel + 2*k2.dvel + 2*k3.dvel + k4.dvel);
}

fragment float4 fs_blackhole(
    VertexOut                 input  [[stage_in ]],
    constant BlackholeParams& params [[buffer(0)]]
){
    float3 relPos = params.camPos - params.bhPos;
    float2 ndc = uv2ndc(input.texCoord);
    float2 ip = ndc2ip(ndc, params.aspect, params.tanHalfFov);
    float3 dir = ip2RayDir(ip, params.camRight, params.camUp, params.camForward);
    GeoState state = initGeoState(relPos, dir, params.rs);

    const float far = 100 * params.rs;
    // float4 far_color = float4(dir * 0.5 + 0.5, 1.0);
    float4 far_color = float4(0.05, 0.05, 0.1, 1);
    // float dl = 0.1;
    const int MAX_STEPS = 150;

    float3 prevPos = state.pos;

    for(int i=0; i<MAX_STEPS; ++i){
        float r = length(prevPos);

        // in event horizon
        if(r <= params.rs)
            return float4(0, 0, 0, 1);

        // far from blackhole
        if(r > far)
            return far_color;

        // adaptive dl
        float dl = 0.3 + 0.7 * (r / far);
        rk4(state, dl, params.rs);
        float3 newPos = state.pos;

        // in disk
        if(prevPos.y * newPos.y < 0.0){
            float t = abs(prevPos.y) / (abs(prevPos.y) + abs(newPos.y));
            float3 y0Pos = mix(prevPos, newPos, t);

            float y0r = length(y0Pos);

            if(params.diskInner <= y0r && y0r <= params.diskOuter){
                float t = y0r / params.diskOuter;
                return float4(1.0, t, 0.2, 1.0);
            }
        }

        prevPos = newPos;
    }

    return far_color;
}
