# 새 이펙트 쓰기

파티클 이펙트 하나는 **`.slang` 파일 하나와 `ParticleEffectDesc` 하나**다. CMake도
C++ 클래스도 필요 없다. 엔진은 슬롯마다 파티클 하나(64바이트)를 GPU 필드에 두고,
녹화되는 프레임마다 한 스텝씩 `cs_step`을 돌린 뒤, 파일에 적은 그리기로 그린다.
예제는 `Engine/Effects/Sample/Island/`의 `Embers.slang`, `Rain.slang`,
`Meteors.slang`이다.

## 1. 파일 하나

`Engine/Effects/Sample/Island/<Name>.slang`:

```slang
// 무엇인지 한두 줄. 그리고 emitter와 params의 뜻:
//
//   emitter    xyz ..., w ...
//   params[0]  x ...; y ...; z ...; w ...

#include "Effect.slang"

// 슬롯 slot이 generation번째로 태어날 때: 위치, 속도, 수명(스텝)
Particle spawn(uint slot, uint generation) {
    Particle p = (Particle)0;
    p.position = effect.emitter.xyz;
    p.velocity = float3(0.0, 1.0, 0.0);
    p.lifeSteps = 60u + effectHash(effect.seed, slot, generation, 1u) % 60u;
    p.generation = generation;
    p.size = 0.02;
    return p;
}

// 한 스텝: 힘을 더하고 움직인다
void update(inout Particle p, uint slot) {
    p.position += p.velocity * effect.dt;
}

struct SparkVertex {
    float4 position : SV_Position;
    float2 corner : TEXCOORD0;
};

// 그리기 "spark": vs_spark와 fs_spark. 파티클 하나에 정점 네 개(strip)
[shader("vertex")]
SparkVertex vs_spark(uint vertexID: SV_VertexID, uint slot: SV_InstanceID) {
    Particle p = loadParticle(slot);
    float2 corner = stripCorner(vertexID);
    SparkVertex o;
    o.position = toClip(billboardCorner(p.position, p.size, corner));
    o.corner = corner;
    return o;
}

[shader("fragment")]
float4 fs_spark(SparkVertex v) : SV_Target {
    float glow = saturate(1.0 - dot(v.corner, v.corner));
    return float4(float3(1.0, 0.6, 0.2) * glow * effect.params[0].x, 1.0);
}

#include "ParticleKernel.slang"
```

`ParticleKernel.slang`은 **맨 마지막에** 넣는다. 위의 `spawn`과 `update`를 부르는
`cs_step`이 거기 있다.

## 2. 설명 한 줄

`Engine/Effects/Sample/Island.cpp`의 `ExtractScene`에서:

```cpp
effects->Add(
    ParticleEffectDesc{
        .name = "sparks",
        .shader = "Engine/Effects/Sample/Island/Sparks.slang",
        .count = 512,
        .seed = 31,
        .prewarmSteps = 120,
        .emitter = Vec4{0.0f, 0.5f, 0.0f, 0.2f},
        .params = {Vec4{8.0f, 0.0f, 0.0f, 0.0f}, Vec4{}, Vec4{}},
        .draws = {{.entry = "spark", .blend = EffectBlend::Additive}}
    }
);
```

- `count`: 슬롯 수. 슬롯 하나가 늘 같은 파티클이고, 수명이 끝나면 `generation + 1`로
  다시 태어난다.
- `prewarmSteps`: 첫 프레임이 보여 주기 전에 미리 도는 스텝 수다. 가장 긴 수명보다
  길게 잡으면 첫 프레임부터 정상 상태다.
- `draws`: 그리기마다 `vs_<entry>`와 `fs_<entry>`를 쓴다. `Additive`는 빛을 더하고(순서
  상관없음), `Alpha`는 뒤에 있는 것 위에 슬롯 순서대로 덮는다.
- 깊이는 장면에 대해 검사하고 쓰지 않는다. `Effects` 패스는 유리(Translucent)보다
  먼저 그려진다.

## 3. 셰이더에서 쓸 수 있는 것

- `effect.count`, `effect.step`, `effect.dt`(1/60), `effect.seed`, `effect.emitter`,
  `effect.params[0..2]`, `effect.cameraRight`, `effect.cameraUp`
- `View.slang`의 `viewProj`, `cameraPosition`
- `particleRandom(slot, generation, k)`: [0, 1) 균등 난수. `effectHash(...)`: 32비트 해시
- `loadParticle(slot)`, `stripCorner(vertexID)`, `billboardCorner(center, halfSize, corner)`,
  `toClip(world)`
- `Particle.custom`: 이펙트가 마음대로 쓰는 float4. `Particle.size`: 보통 반지름

## 4. 규칙

- **시간은 `effect.step * effect.dt`**, 또는 `p.ageSteps * effect.dt`다. 벽시계는 쓰지
  않는다. 그래서 프레임 N은 매번 같은 그림이고, 골든이 결정적이다.
- **난수는 `particleRandom(slot, generation, k)`로만** 만든다. k는 작은 수로 쓴다.
  `0xA6E`는 첫 스텝의 나이 분산이 쓰는 값이다.
- **원자 연산과 readback은 쓰지 않는다.** GPU 결과는 게임플레이로 돌아가지 않는다.
- **한 파일에 push struct는 하나만 둔다.** 이펙트는 `EffectPush`만 쓴다. 다른 push가
  있는 셰이더와 한 파일에 섞지 않는다(Metal).
- 수명 대신 사건으로 다시 태어나게 하려면 `Rain.slang`처럼 한다. `lifeSteps`를
  `0xFFFFFFFFu`로 두고, `update`에서 `p.generation`을 올리고 새 자리로 옮긴다.
- 안 보이는 슬롯은 버텍스 단계에서 크기 0으로 접는다(`Rain.slang`의 물결,
  `Meteors.slang`의 쉬는 시간).

## 5. 돌려 보기

```bash
powershell -NoProfile -File Tools/build.ps1 -Config Debug -Target Island
```

```bash
build/bin/Island.exe
```

- 실행 중에는 포트의 `reload_shaders`가 그리기(`vs_`/`fs_`)를 다시 읽는다. `spawn`과
  `update`(`cs_step`)는 다시 실행해야 바뀐다.
- 그림이 마음에 들면 smoke로 찍고, 실패가 출력한 `Copy-Item` 줄로 골든을 받아들인다.

```bash
powershell -NoProfile -File Tools/smoke_run.ps1 build\bin\Island.exe
```

## 6. 파티클이 아닌 것

바다 물결이나 연기 같은 격자는 한 층 아래에서 만든다.
- `FieldBuffer`: 프레임 사이에 남는 GPU 버퍼
- `ComputeKernel`: 컴퓨트 진입점 하나
- `FieldPass`: 획득과 해제, 디스패치 사이 배리어, 처음 쓰는 필드의 클리어

예제는 `Engine/Effects/Check/FieldCheck.cpp`다. 2D·3D 텍스처 필드와 핑퐁은 아직
없다(설계만 있다).
