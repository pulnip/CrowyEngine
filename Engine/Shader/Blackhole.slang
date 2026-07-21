#include "Sampler.slang"

// [-1, 1] to Image Plane
float2 ndc2ip(float2 ndc, float aspect, float tanHalfFov){
    return ndc * float2(aspect, 1.0) * tanHalfFov;
}

float3 ip2RayDir(float2 ip, float3 right, float3 up, float3 forward){
    return normalize(ip.x * right + ip.y * up + forward);
}

struct GeoDerives {
    float3 dpos;
    float3 dvel;

    __init(float3 dpos, float3 dvel) {
        this.dpos = dpos;
        this.dvel = dvel;
    }
};

struct GeoState {
    float3 pos;
    float3 vel;

    __init(float3 pos, float3 vel) {
        this.pos = pos;
        this.vel = vel;
    }

    GeoDerives geodesicRHS() {
        float r = length(pos);
        float3 nhat = pos / r;
        float vr = dot(vel, nhat);
        float vperp_sq = dot(vel, vel) - vr * vr;

        return GeoDerives(
            vel,
            -1.5 * rs / (r * r) * vperp_sq * nhat
        );
    }

    GeoState applyDerives(GeoDerives d, float h) {
        return GeoState(
            pos + d.dpos * h,
            vel + d.dvel * h
        );
    }
};

void rk4(inout GeoState s, float dl) {
    GeoDerives k1 = s.geodesicRHS();
    GeoDerives k2 = s.applyDerives(k1, 0.5 * dl).geodesicRHS();
    GeoDerives k3 = s.applyDerives(k2, 0.5 * dl).geodesicRHS();
    GeoDerives k4 = s.applyDerives(k3,       dl).geodesicRHS();

    s.pos += (dl / 6.0) * (k1.dpos + 2 * k2.dpos + 2 * k3.dpos + k4.dpos);
    s.vel += (dl / 6.0) * (k1.dvel + 2 * k2.dvel + 2 * k3.dvel + k4.dvel);
}

struct VertexOut {
    float4 position : SV_Position;
    float2 ndc : TEXCOORD0;
};

[shader("vertex")]
VertexOut vs_main(uint vertexID: SV_VertexID) {
    // TriangleStrip, Clockwise to Front-Face
    static const float2 ndc[4] = {
        float2(-1.0, 1.0), float2(1.0, 1.0),
        float2(-1.0, -1.0), float2(1.0, -1.0)
    };

    VertexOut output;
    output.position = float4(ndc[vertexID], 0, 1);
    output.ndc = ndc[vertexID];

    return output;
}

struct ResourceData {
    DescriptorHandle<Texture2D> disk;
};
[[vk::push_constant]]
ConstantBuffer<ResourceData> resource;

static const int PLANET_COUNT = 3;
cbuffer params : register(b1) {
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
    float4 posRadius[PLANET_COUNT];
    float4 planetColor[PLANET_COUNT];
    float elapsedTimeSeconds;
};

float3 sampleDiskWithDoppler(
    float3 hitPos,
    float t,
    float3 rayDir,
    float rs,
    float diskInner,
    float diskOuter,
    Texture2D disk,
    SamplerState linearWrap
) {
    float r = length(hitPos);
    float theta = atan2(hitPos.z, hitPos.x);

    // kepler's Orbital velocity
    float v_orb = sqrt(rs / (2.0 * r));

    // rotate anti-clockwise
    float3 orbitalDir = normalize(float3(-hitPos.z, 0, hitPos.x));
    // rotate clockwise
    // float3 orbitalDir = normalize(float3(hitPos.z, 0, -hitPos.x));

    // doppler factor
    float dopplerCosAngle = dot(rayDir, orbitalDir);
    float doppler = 1.0 / (1.0 + v_orb * dopplerCosAngle);

    // redshift factor (gravitational + doppler)
    float redShift = doppler * sqrt(max(1.0 - rs / r, 0.01));

    float u = (r - diskInner) / (diskOuter - diskInner);
    float omega = pow(diskInner / r, 1.5);
    float v = fract(theta / (2.0 * float.getPi()) + 0.2 * omega * t);

    // g^3 boosting
    return pow(redShift, 3) * disk.Sample(linearWrap, float2(u, v)).rgb;
}

struct FragmentOut {
    float4 scene : COLOR0;
    float4 brightMask : COLOR1;
};

[shader("fragment")]
FragmentOut fs_main(VertexOut input) : SV_Target {
    static const float4 far_color = float4(0.05, 0.05, 0.1, 1);
    FragmentOut output;
    output.scene = far_color;
    output.brightMask = float4(0, 0, 0, 1);

    float2 ip = ndc2ip(input.ndc, aspect, tanHalfFov);
    float3 dir = ip2RayDir(ip, camRight, camUp, camForward);
    GeoState state = GeoState(camPos - bhPos, dir);

    const float far_ = 100 * rs;
    static const int MAX_STEPS = 150;

    float3 prevPos = state.pos;

    for (int i = 0; i < MAX_STEPS; ++i) {
        float r = length(prevPos);

        for (int j = 0; j < PLANET_COUNT; ++j) {
            float3 pPos = posRadius[j].xyz;
            float pRad = posRadius[j].w;
            float3 pToRay = prevPos - pPos;
            // in planet
            if (length(pToRay) <= pRad) {
                float3 N = normalize(pToRay);
                float3 L = normalize(camPos - prevPos);
                float diff = max(dot(N, L), 0.1);

                output.scene = float4(diff * planetColor[j].rgb, 1);
                return output;
            }
        }

        // in event horizon
        if (r <= rs) {
            output.scene = float4(0, 0, 0, 1);
            return output;
        }

        // far from blackhole
        if (r > far_) {
            // scene = far_color
            return output;
        }

        // adaptive dl
        float dl = 0.3 + 0.7 * (r / far_);
        rk4(state, dl);
        float3 newPos = state.pos;

        // in disk
        if (prevPos.y * newPos.y < 0.0) {
            float t = abs(prevPos.y) / (abs(prevPos.y) + abs(newPos.y));
            float3 y0Pos = lerp(prevPos, newPos, t);

            float y0r = length(y0Pos);

            if (diskInner <= y0r && y0r <= diskOuter) {
                // float t2 = y0r / diskOuter;
                // float3 diskColor = float3(1.0, t2, 0.2);
                float3 diskColor = sampleDiskWithDoppler(
                    y0Pos,
                    elapsedTimeSeconds,
                    dir,
                    rs,
                    diskInner,
                    diskOuter,
                    resource.disk,
                    linearWrap
                );
                output.scene = float4(diskColor, 1.0);

                float brightness = dot(diskColor, float3(0.2126, 0.7152, 0.0722));
                output.brightMask = brightness > 0.3 ?
                    float4(diskColor, 1) :
                    float4(0, 0, 0, 1);

                return output;
            }
        }

        prevPos = newPos;
    }

    return output;
}
