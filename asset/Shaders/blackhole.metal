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
    // phi for x-z plane angle, theta for polar(+y) angle.
    float  r,  theta,  phi;
    float dr, dtheta, dphi;
    float E;
};

GeoState initGeoState(float3 pos, float3 dir, float rs){
    GeoState s;
    s.r     = length(pos);
    s.theta = acos(pos.y / s.r);
    s.phi   = atan2(pos.z, pos.x);

    float sint = sin(s.theta), cost = cos(s.theta);
    float sinp = sin(s.phi  ), cosp = cos(s.phi  );

    s.dr     =  sint*cosp*dir.x + cost*dir.y + sint*sinp*dir.z;
    s.dtheta = (cost*cosp*dir.x - sint*dir.y + cost*sinp*dir.z) / s.r;
    s.dphi   = (    -sinp*dir.x + cosp*dir.z) / (s.r * sint);

    float f = 1.0 - rs / s.r;
    float dt_dl = sqrt(
        (s.dr*s.dr) / f +
        ( s.r* s.r) * (s.dtheta*s.dtheta + sint*sint*s.dphi*s.dphi)
    );
    s.E = f * dt_dl;

    return s;
}

struct GeoDerives{
    float  dr,  dtheta,  dphi;
    float ddr, ddtheta, ddphi;
};

GeoDerives geodesicRHS(GeoState s, float rs){
    float r = s.r;
    float f = 1.0 - rs / r;
    float dt_dl = s.E / f;
    float sint = sin(s.theta), cost = cos(s.theta);

    GeoDerives d;
    d.dr     = s.dr;
    d.dtheta = s.dtheta;
    d.dphi   = s.dphi;

    d.ddr = -(rs / (2*r*r)) * f * dt_dl*dt_dl +
            (rs / (2*r*r*f)) * s.dr*s.dr +
            r * (s.dtheta*s.dtheta + sint*sint * s.dphi*s.dphi);
    d.ddtheta = -(2.0/r) * s.dr * s.dtheta +
                sint*cost * s.dphi*s.dphi;
    d.ddphi = -(2.0/r) * s.dr * s.dphi
              - 2.0*(cost/sint) * s.dtheta*s.dphi;

    return d;
}

GeoState applyDerives(GeoState s, GeoDerives d, float h){
    GeoState n;

    n.r     = s.r     + d.dr     * h;
    n.theta = s.theta + d.dtheta * h;
    n.phi   =  s.phi  + d.dphi   * h;

    n.dr     = s.dr     + d.ddr     * h;
    n.dtheta = s.dtheta + d.ddtheta * h;
    n.dphi   = s.dphi   + d.ddphi   * h;

    n.E = s.E;

    return n;
}

void rk4(thread GeoState& s, float dl, float rs){
    GeoDerives k1 = geodesicRHS(                          s, rs);
    GeoDerives k2 = geodesicRHS(applyDerives(s, k1, 0.5*dl), rs);
    GeoDerives k3 = geodesicRHS(applyDerives(s, k2, 0.5*dl), rs);
    GeoDerives k4 = geodesicRHS(applyDerives(s, k3,     dl), rs);

    s.r      += (dl/6.0) * (k1.dr      + 2*k2.dr      + 2*k3.dr      + k4.dr);
    s.theta  += (dl/6.0) * (k1.dtheta  + 2*k2.dtheta  + 2*k3.dtheta  + k4.dtheta);
    s.phi    += (dl/6.0) * (k1.dphi    + 2*k2.dphi    + 2*k3.dphi    + k4.dphi);
    s.dr     += (dl/6.0) * (k1.ddr     + 2*k2.ddr     + 2*k3.ddr     + k4.ddr);
    s.dtheta += (dl/6.0) * (k1.ddtheta + 2*k2.ddtheta + 2*k3.ddtheta + k4.ddtheta);
    s.dphi   += (dl/6.0) * (k1.ddphi   + 2*k2.ddphi   + 2*k3.ddphi   + k4.ddphi);
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
    const int MAX_STEPS = 200;

    float3 prevPos = sphericalToCartesian(state.r, state.theta, state.phi);

    for(int i=0; i<MAX_STEPS; ++i){
        // in event horizon
        if(state.r <= params.rs)
            return float4(0, 0, 0, 1);

        // adaptive dl
        float dl = 0.1 + 0.9 * (state.r / far);
        rk4(state, dl, params.rs);
        float3 newPos = sphericalToCartesian(state.r, state.theta, state.phi);

        // in disk
        if(prevPos.y * newPos.y < 0.0){
            float t = abs(prevPos.y) / (abs(prevPos.y) + abs(newPos.y));
            float3 y0Pos = mix(prevPos, newPos, t);

            float r = length(y0Pos);

            if(params.diskInner <= r && r <= params.diskOuter){
                float t = r / params.diskOuter;
                return float4(1.0, t, 0.2, 1.0);
            }
        }

        // far from blackhole
        if(state.r > far)
            return far_color;

        prevPos = newPos;
    }

    return far_color;
}
