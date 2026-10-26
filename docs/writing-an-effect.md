# 새 이펙트 쓰기

파티클 이펙트 하나는 **`.slang` 파일 하나와 `ParticleEffectDesc` 하나**다. CMake도
C++ 클래스도 필요 없다. 엔진은 슬롯마다 파티클 하나(64바이트)를 GPU 필드에 두고,
1/60초 스텝마다 `cs_step`을 한 번 돌리고, 파일에 적은 그리기로 그린다. 한 프레임에
몇 스텝을 돌지는 샘플이 정한다. Island는 World의 고정 틱을 그대로 쓴다(120 Hz
화면이면 두 프레임에 한 틱). 프레임을 찍는 실행(`--capture-frame`, smoke가 이렇게
찍는다)은 앱이 프레임마다 정확히 한 틱을 내서, 60번째 프레임이 매번 같은 그림이다.
예제는 `Engine/Effects/Sample/Island/`의 `.slang` 파일들이다.

## 1. 파일 하나

`Engine/Effects/Sample/Island/<Name>.slang`:

```slang
// 무엇인지 한두 줄

#include "Effect.slang"

// desc의 origin, size, params가 여기서 뜻하는 것: 이름이 설명이다
float3 source() { return effect.origin.xyz; }
float brightness() { return effect.params[0].x; }

// 슬롯 slot이 generation번째로 태어날 때: 위치, 속도, 수명(스텝)
Particle spawn(uint slot, uint generation) {
    Particle p = (Particle)0;
    p.position = source();
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
    return float4(float3(1.0, 0.6, 0.2) * glow * brightness(), 1.0);
}

#include "ParticleKernel.slang"
```

`ParticleKernel.slang`은 **맨 마지막에** 넣는다. 위의 `spawn`과 `update`를 부르는
`cs_step`이 거기 있다.

## 2. 설명 한 줄

`Engine/Effects/Sample/Island/IslandEffects.hpp`에 desc 함수를 하나 더하고:

```cpp
inline ParticleEffectDesc sparksDesc() {
    return ParticleEffectDesc{
        .name = "sparks",
        .shader = "Engine/Effects/Sample/Island/Sparks.slang",
        .count = 512,
        .seed = 31,
        .prewarmSteps = 120,
        .origin = Vec3{0.0f, 0.5f, 0.0f},
        .size = 0.2f,
        .params = {Vec4{8.0f, 0.0f, 0.0f, 0.0f}},
        .draws = {{.entry = "spark", .blend = EffectBlend::Additive}}
    };
}
```

`Engine/Effects/Sample/Island.cpp`의 `ExtractScene`에서 `effects->Add(sparksDesc());`
한 줄을 더한다.

- `origin`과 `size`: 이펙트가 있는 자리와, 거기서 닿는 거리(반지름, 반높이 같은 것)다.
  이펙트를 옮기는 쪽은 `origin`만 바꾸고 `size`는 그대로 둔다. 하늘의 별과 유성처럼
  자리가 없는 이펙트는 `origin`을 읽지 않는다.
- 이펙트가 액터를 따라가게 하려면 액터에 `EffectComponent{"이름"}`을 단다
  (`Engine/WorldEffects`). 그 액터가 놓이거나 움직인 프레임의 Extract가 `origin`을 액터의
  월드 위치로 쓴다. Island의 불꽃과 불씨가 불 액터 아래에 이렇게 달려 있다. 옮긴
  `origin`은 다음 탄생부터 쓰이고, 이미 태어난 파티클은 제자리다. 셰이더가 월드 좌표를
  따로 쓰면(불씨의 티피 굴뚝) 그 부분은 따라가지 않는다.
- `params[0..3]`: 이펙트가 정하는 float4 네 개다. 뜻은 파일 머리의 접근자 함수가
  이름으로 말한다(`Rain.slang`의 `fallSpeed()`, `ringOpacity()`처럼). 자리가 아닌 값은
  `origin`이 아니라 여기에 둔다.
- `count`: 슬롯 수. 슬롯 하나가 늘 같은 파티클이고, 수명이 끝나면 `generation + 1`로
  다시 태어난다.
- `prewarmSteps`: 첫 프레임이 보여 주기 전에 미리 도는 스텝 수다. 가장 긴 수명보다
  길게 잡으면 첫 프레임부터 정상 상태다.
- `draws`: 그리기마다 `vs_<entry>`와 `fs_<entry>`를 쓴다. `Additive`는 빛을 더하고(순서
  상관없음), `Alpha`는 슬롯 순서대로 덮는다. 이펙트는 깊이를 쓰지 않으니 `Alpha`는 먼저
  그려진 이펙트를 앞뒤 없이 모두 덮는다. 그래서 `Alpha` 그리기는 `Additive`보다 앞에
  두고, `Alpha`가 있는 이펙트를 먼저 `Add`한다(Island의 순서는 `Island.cpp`의
  `ExtractScene`에 있는 `Add` 호출 순서다).
- 깊이는 장면에 대해 검사하고 쓰지 않는다. `Effects` 패스는 엔진의 표준 파이프라인에
  없다. Island가 자기 `DescribePipeline`에서 유리(Translucent) 앞에 끼워 넣는 hook 패스다.

## 3. 셰이더에서 쓸 수 있는 것

- `effect.count`, `effect.step`, `effect.worldStep`, `effect.dt`(1/60), `effect.seed`,
  `effect.origin.xyz`, `effect.size`, `effect.params[0..3]`, `effect.cameraRight`,
  `effect.cameraUp`
- `loopPhase(step, cycles)`: `LoopSteps`(`WorldClock.slang`) 루프를 `cycles`번 도는 것의
  위상, [0, 1)
- `View.slang`의 `viewProj`, `cameraPosition`
- `particleRandom(slot, generation, k)`: [0, 1) 균등 난수. `effectHash(...)`: 32비트 해시
- `loadParticle(slot)`, `stripCorner(vertexID)`, `billboardCorner(center, halfSize, corner)`,
  `toClip(world)`
- `EventLife`: 나이로는 닿지 않는 수명(아래 사건으로 다시 태어나기)
- `Particle.custom`: 이펙트가 마음대로 쓰는 float4. `Particle.size`: 보통 반지름
- Island의 공유 파일(같은 폴더라 `#include "..."`로 바로 읽힌다):
  - `IslandScene.h`: 섬, 달, 하늘, 불과 삼각대, 티피, 바람의 숫자. C++도 같은 파일을
    읽는다.
  - `IslandShapes.slang`: `islandCrown(xz)`, `tipiRoof(xz)`, `tipiReach`, `tipiApothem`.
    비가 앉는 면, 불씨가 갇히는 티피가 여기 있다.
  - `Sea.slang`: `seaHeight(xz, step, count, onGrid)`. 비가 앉는 파도가 여기 있다.
  - `Weather.slang`: `fireBreath(effect.worldStep)`과 `windAt(xz, effect.worldStep)`.
    불빛과 불꽃이 같이 숨 쉬고, 비와 불씨와 불꽃이 같은 바람을 받는다. 바람은 자리와
    world step만의 함수라서, C++(`Weather.hpp`)도 같은 자리 같은 스텝이면 허용 오차 안의
    같은 값을 낸다(`IslandCheck`가 비의 낙하를 그렇게 다시 계산한다). 정확히 같지는
    않으니 CPU 쪽 비교에는 늘 허용 오차를 둔다.

## 4. 규칙

- **시간은 스텝으로만 센다.** 셰이더에서 벽시계는 쓰지 않는다. 스텝을 언제 돌릴지만
  샘플이 시간으로 정하고, 찍을 때는 프레임으로 센다. 그래서 골든이 결정적이다.
  - 장면이나 다른 이펙트와 맞아야 하는 것(바람, 바다, 불빛의 숨)은 `effect.worldStep`을
    쓴다. 모든 이펙트와 장면이 같은 값을 본다. prewarm은 거기서 거꾸로 센다.
  - 이펙트 자기만의 역사(탄생, 나이)는 `effect.step`과 `p.ageSteps`다. 이펙트마다
    prewarm만큼 다르다.
  - 주기가 있는 것은 `loopPhase(effect.worldStep, cycles)`로 위상을 만든다. 루프가
    2^32를 나누므로 prewarm에서 0 아래로 감긴 스텝도 같은 위상이 된다.
- **난수는 `particleRandom(slot, generation, k)`나 `effectHash(effect.seed, slot,
  generation, k)`, 그 [0, 1) 판인 `effectRandom(effect.seed, slot, generation, k)`로만**
  만든다. k는 작은 수로 쓴다. `ParticleStaggerDraw`(`Effect.slang`)는 첫 스텝의 나이
  분산이 쓰는 k다. 시간에 따라
  바뀌는 난수는 generation 자리에 world step을 나눈 값을 넣는다(`Stars.slang`의 반짝임).
- **원자 연산과 readback은 쓰지 않는다.** GPU 결과는 게임플레이로 돌아가지 않는다.
- **한 파일에 push struct는 하나만 둔다.** 이펙트는 `EffectPush`만 쓴다. 다른 push가
  있는 셰이더와 한 파일에 섞지 않는다(Metal).
- 수명 대신 사건으로 다시 태어나게 하려면 `Rain.slang`처럼 한다. `lifeSteps`를
  `EventLife`로 두고, `update`에서 `p.generation`을 올리고 `p.ageSteps = 0u`로
  되돌린 뒤 새 자리로 옮긴다. 첫 스텝은 이런 파티클의 나이를 흩지 않으므로,
  `ageSteps`는 태어난 뒤나 마지막 사건 뒤의 스텝 수다.
- 안 보이는 슬롯은 버텍스 단계에서 크기 0으로 접는다(`Rain.slang`의 물결,
  `Meteors.slang`의 쉬는 시간).
- 그리기는 같은 파티클을 여러 번 그려도 된다. `Meteors.slang`은 꼬리, 머리, 점화 섬광을
  세 그리기로 나누고, 블룸 없이도 빛나 보이게 글로와 십자 광선을 스프라이트 안에 그린다.
  볼리의 뒤쪽 유성들은 버텍스 단계에서 앞장선 슬롯의 파티클을 읽어 따라간다.
- 하늘에 붙은 것은 방향만 저장하고, 버텍스 단계에서 `cameraPosition`으로부터 일정한
  거리(far plane 안쪽)에 놓는다. 그러면 카메라를 따라오고 시차가 없다(`Stars.slang`,
  `Meteors.slang`). 움직임도 나이의 함수로 계산하면 `update`가 비어도 된다.

## 5. 돌려 보기

```bash
powershell -NoProfile -File Tools/build.ps1 -Config Debug -Target Island
```

```bash
build/bin/Debug/Island.exe
```

- 실행 중에 포트의 `reload_shaders`를 부르면 그리기(`vs_`/`fs_`)와 `cs_step`(`spawn`,
  `update`)을 모두 다시 읽는다. 하나라도 컴파일에 실패하면 전부 옛 것으로 남고, 오류가
  답으로 돌아온다. 이미 태어난 파티클은 그대로이고, 바뀐 `spawn`은 다음 탄생부터 적용된다.

```powershell
. Tools/port.ps1; Invoke-Port reload_shaders
```
- 포트는 `CROWY_COMMAND_PORT`(없으면 27500)다. Island를 띄울 때와 같은 값이어야 한다.
- 그림이 마음에 들면 smoke로 프레임 60을 `captures/Island.png`에 찍는다.
  `Engine/Effects/Sample/Golden/Island.<dx12|metal>.png`는 골든이 아니라 기준선이라서
  smoke와 ctest는 비교하지 않는다. 그림이 그대로인지는 `ImageCompareCheck`로 손수
  비교하고, 기준선은 오너가 새 모습을 받아들일 때만 다시 찍는다(`docs/testing.md`).

```bash
CROWY_SMOKE_CAPTURE_DIR=captures powershell -NoProfile -File Tools/smoke_run.ps1 build/bin/Debug/Island.exe
```

```bash
build/bin/Debug/ImageCompareCheck.exe captures/Island.png Engine/Effects/Sample/Golden/Island.dx12.png --tolerance 0
```

## 6. 파티클이 아닌 것

Island의 바다는 파티클도 필드도 아니고 머티리얼이다. `Ocean.slang`의 `vs_ocean`이
섬을 둘러싼 극좌표 격자를 긴 파도 `SEA_VERTEX_WAVES`개로 들어 올린다. `fs_ocean`은
그 픽셀이 감당하는 만큼의 파도로 노멀을 만들고, 밤하늘의 반사(emissive)와 물가의
거품(albedo, roughness)을 `Surface`에 넣은 뒤 엔진의 빛 루프(`sceneColor`)로 달과
물가의 불빛 반사를 얻는다. 그래서 Normals 같은 데이터 뷰도 그대로 맞다.
- 파도는 `Sea.slang`의 `seaHeight(xz, step, count, onGrid)`다. 날카로운 마루의 파도
  `SEA_WAVE_COUNT`개(`SeaWaves.h`, C++도 같은 표를 읽는다)를 바람 방향으로 흘린다.
  셰이더토이의 Seascape(CC BY-NC-SA)와 afl_ext의 바다(2023판 MIT)에서 기법을 배웠고
  코드는 가져오지 않았다. 파장 비 1.18과 정점·픽셀 파도 수의 나눔은 afl_ext의 값이고,
  높이와 방향과 속도는 물리에서 새로 정했다.
- 머티리얼의 레인 두 개로 시간과 달을 받는다: `custom0.x`가 루프 안의 world step,
  `custom1.xyz`가 달 쪽 방향이다. `Island.cpp`가 step은 프레임마다, 달은 만들 때 한 번
  쓴다.
- 비는 같은 `seaHeight`(격자 파도 `SEA_VERTEX_WAVES`개)에 떨어지고, 물결 고리는 파도를 타고,
  `IslandCheck`의 CPU 트윈(`Sea.hpp`)이 같은 높이를 계산한다.

바다 물결의 시뮬레이션이나 연기 같은 격자는 한 층 아래에서 만든다.
- `FieldBuffer`: 프레임 사이에 남는 GPU 버퍼
- `ComputeKernel`: 컴퓨트 진입점 하나
- `FieldPass`: 획득과 해제, 디스패치 사이 배리어, 처음 쓰는 필드의 클리어

예제는 `Engine/Effects/Check/FieldCheck.cpp`다.
