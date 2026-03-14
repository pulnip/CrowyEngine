#include <metal_stdlib>

using namespace metal;

// [-1, 1] to Image Plane
float2 ndc2ip(float2 ndc, float aspect, float tanHalfFov){
    return ndc * float2(aspect, 1.0) * tanHalfFov;
}

float3 ip2RayDir(float2 ip, float3 right, float3 up, float3 forward){
    return normalize(ip.x * right + ip.y * up + forward);
}

struct VertexOutNDC{
    float4 position [[position]];
    float2 ndc;
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

struct PlanetParams{
    static constant constexpr int COUNT = 3;
    float4 posRadius[COUNT];
    float4 color[COUNT];
};

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
    VertexOutNDC           input [[stage_in ]],
    constant BlackholeParams& bh [[buffer(0)]],
    constant PlanetParams&    pn [[buffer(1)]]
){
    float2 ip = ndc2ip(input.ndc, bh.aspect, bh.tanHalfFov);
    float3 dir = ip2RayDir(ip, bh.camRight, bh.camUp, bh.camForward);
    GeoState state = initGeoState(bh.camPos - bh.bhPos, dir, bh.rs);

    const float far = 100 * bh.rs;
    // float4 far_color = float4(dir * 0.5 + 0.5, 1.0);
    float4 far_color = float4(0.05, 0.05, 0.1, 1);
    // float dl = 0.1;
    const int MAX_STEPS = 150;

    float3 prevPos = state.pos;

    for(int i=0; i<MAX_STEPS; ++i){
        float r = length(prevPos);

        for(int j=0; j<pn.COUNT; ++j){
            float3 pPos = pn.posRadius[j].xyz;
            float pRad = pn.posRadius[j].w;
            float3 pToRay = prevPos - pPos;
            // in planet
            if(length(pToRay) <= pRad){
                float3 N = normalize(pToRay);
                float3 L = normalize(bh.camPos - prevPos);
                float diff = max(dot(N, L), 0.1);
                return float4(diff * pn.color[j].rgb, 1);
            }
        }

        // in event horizon
        if(r <= bh.rs)
            return float4(0, 0, 0, 1);

        // far from blackhole
        if(r > far)
            return far_color;

        // adaptive dl
        float dl = 0.3 + 0.7 * (r / far);
        rk4(state, dl, bh.rs);
        float3 newPos = state.pos;

        // in disk
        if(prevPos.y * newPos.y < 0.0){
            float t = abs(prevPos.y) / (abs(prevPos.y) + abs(newPos.y));
            float3 y0Pos = mix(prevPos, newPos, t);

            float y0r = length(y0Pos);

            if(bh.diskInner <= y0r && y0r <= bh.diskOuter){
                float t = y0r / bh.diskOuter;
                return float4(1.0, t, 0.2, 1.0);
            }
        }

        prevPos = newPos;
    }

    return far_color;
}
