// [-1, 1] to Image Plane
float2 ndc2ip(float2 ndc, float aspect, float tanHalfFov){
    return ndc * float2(aspect, 1.0) * tanHalfFov;
}

float3 ip2RayDir(float2 ip, float3 right, float3 up, float3 forward){
    return normalize(ip.x * right + ip.y * up + forward);
}

struct VertexOutNDC{
    float4 position : SV_Position;
    float2 ndc      : TEXCOORD0;
};

cbuffer BlackholeParams : register(b0){
    float3 bhPos;
    float rs;
    float3 camPos;
    float aspect;
    float3 camRight;
    float tanHalfFov;
    float3 camUp;
    float diskInner;
    float3 camForward;
    float diskOuter;
};

static const int PLANET_COUNT = 3;

cbuffer PlanetParams : register(b1){
    float4 posRadius[PLANET_COUNT];
    float4 planetColor[PLANET_COUNT];
};

struct GeoState{
    float3 pos;
    float3 vel;
};

GeoState initGeoState(float3 pos, float3 dir){
    GeoState s;
    s.pos = pos;
    s.vel = dir;

    return s;
}

struct GeoDerives{
    float3 dpos;
    float3 dvel;
};

GeoDerives geodesicRHS(GeoState s){
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

void rk4(inout GeoState s, float dl){
    GeoDerives k1 = geodesicRHS(                          s);
    GeoDerives k2 = geodesicRHS(applyDerives(s, k1, 0.5*dl));
    GeoDerives k3 = geodesicRHS(applyDerives(s, k2, 0.5*dl));
    GeoDerives k4 = geodesicRHS(applyDerives(s, k3,     dl));

    s.pos += (dl/6.0) * (k1.dpos + 2*k2.dpos + 2*k3.dpos + k4.dpos);
    s.vel += (dl/6.0) * (k1.dvel + 2*k2.dvel + 2*k3.dvel + k4.dvel);
}

float4 fs_blackhole(VertexOutNDC input) : SV_Target{
    float2 ip = ndc2ip(input.ndc, aspect, tanHalfFov);
    float3 dir = ip2RayDir(ip, camRight, camUp, camForward);
    GeoState state = initGeoState(camPos - bhPos, dir);

    const float far_ = 100 * rs;
    float4 far_color = float4(0.05, 0.05, 0.1, 1);
    static const int MAX_STEPS = 150;

    float3 prevPos = state.pos;

    for(int i = 0; i < MAX_STEPS; ++i){
        float r = length(prevPos);

        for(int j = 0; j < PLANET_COUNT; ++j){
            float3 pPos = posRadius[j].xyz;
            float  pRad = posRadius[j].w;
            float3 pToRay = prevPos - pPos;
            // in planet
            if(length(pToRay) <= pRad){
                float3 N = normalize(pToRay);
                float3 L = normalize(camPos - prevPos);
                float diff = max(dot(N, L), 0.1);
                return float4(diff * planetColor[j].rgb, 1);
            }
        }

        // in event horizon
        if(r <= rs)
            return float4(0, 0, 0, 1);

        // far from blackhole
        if(r > far_)
            return far_color;

        // adaptive dl
        float dl = 0.3 + 0.7 * (r / far_);
        rk4(state, dl);
        float3 newPos = state.pos;

        // in disk
        if(prevPos.y * newPos.y < 0.0){
            float t = abs(prevPos.y) / (abs(prevPos.y) + abs(newPos.y));
            float3 y0Pos = lerp(prevPos, newPos, t);

            float y0r = length(y0Pos);

            if(diskInner <= y0r && y0r <= diskOuter){
                float t2 = y0r / diskOuter;
                return float4(1.0, t2, 0.2, 1.0);
            }
        }

        prevPos = newPos;
    }

    return far_color;
}
