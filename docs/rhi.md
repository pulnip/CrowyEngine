# RHI

D3D12와 Metal의 호출을 단일한 gfx api로 가능하게 만들었다.
렌더러(`Render`)랑 샘플은 백엔드 API 대신 이 RHI를 호출한다.

## 구조

- **가상 인터페이스.**
  `RHIDevice`, `RHICommandList`, `RHITexture`, `RHIBuffer`, `RHISwapchain`, `RHIGraphicsPipelineState`, `RHIComputePipelineState`, `RHIFence`가 추상 클래스로 있다([Engine/RHI/Public](../Engine/RHI/Public)).
  구현은 `DX12Device`([DX12RHI](../Engine/DX12RHI))랑 `MetalDevice`([MetalRHI](../Engine/MetalRHI))가 한다.
- **백엔드는 플랫폼마다 하나, 컴파일 타임에 정해진다.**
  CMake가 Windows에선 DX12RHI를, macOS에선 MetalRHI를 링크하고 `createDevice()`의 기본값도 `#if`로 갈린다([RHIDevice.hpp](../Engine/RHI/Public/RHIDevice.hpp)).
  한 플랫폼 내에서 두 백엔드가 가능한 상황이 없으므로 런타임에 백엔드를 바꾸는 건 현재 지원하지 않는다.
- **디버그 빌드에서는 base 클래스가 검증을 한다.**
  `RHICommandList`의 가상 함수 기본 구현이 패스 상태를 검사한다. 패스 안에서 패스를 열었는지, 짝이 안 맞는 End인지, 배리어 형식이 맞는지.
  통계도 센다.
  따라서, 각 백엔드(RHI 구현체)는 이걸 먼저 부르고 자기 일을 한다.
- **프레임 3개 in flight.**
  `FramePacer`, `CommandListPool`, transient 업로드 링(`AllocateTransient` / `UploadTransient`)이 공통이다.
  지운 리소스는 `DeferRetire`로 GPU가 다 쓴 뒤에 회수한다.

## 배리어: 패스 사이의 edge

D3D12 Enhanced Barrier를 기준으로 설계했다([RHIDefinitions.hpp](../Engine/RHI/Common/RHIDefinitions.hpp)).
배리어 하나를 **두 패스 사이의 edge**로 보고, (store-release / load-acquire처럼) 나눠서 실어 보낸다. (정확히는, signal & wait 모델)

- **release**: 앞 패스는 리소스 접근을 모두 마친 뒤 `EndRenderPass(releases)`에서 edge를 알린다.
- **acquire**: 뒤 패스가 `BeginRenderPass(desc, acquires)`에서 그 edge를 기다린 뒤에 리소스에 접근한다.
- 양쪽 다 같은 edge 값(`makeBarrier(texture, 이전 용도, 다음 용도)`)을 들고 있다.

```cpp
// 시뮬레이션 패스가 scene을 다 그리고 release
const auto sceneEdge = makeBarrier(*scene, RenderTarget, SampledFragment);
cmdList.EndRenderPass(std::array{sceneEdge, brightMaskEdge});

bloomPass.Execute(*brightMask, cmdList);   // scene은 안 건드림

// 합성 패스가 같은 edge를 acquire
cmdList.BeginRenderPass(compositeDesc, std::array{sceneEdge, bloomEdge});
```

**인접하지 않은 패스 사이의 의존성**을 표현할 수 있다는 것이 장점인데, 블랙홀 샘플이 그 예시다([BlackholeSimulation.cpp](../Engine/RHI/Sample/BlackholeSimulation.cpp)).

- `scene` 텍스처는 시뮬레이션 패스가 release하고 그 사이에 있는 bloom 체인 전체를 건너뛰어서 합성 패스에서야 acquire된다.

백엔드 별 차이도 RHI가 흡수한다. (D3D12는 barrier가 3축, Metal은 2축)

- D3D12: 실제 `D3D12_BARRIER_GROUP`으로 나간다.
- Metal: release는 producer 인코더가 마지막 스테이지 뒤에 fence를 update하고 acquire는 consumer 인코더가 첫 스테이지 전에 그 fence를 wait한다([MetalCommandList.cpp](../Engine/MetalRHI/Private/MetalCommandList.cpp)).

패스는 render, compute, blit 세 종류로 나뉘고, 렌더러 쪽 `FramePipeline`은 각 패스의 입출력을 보고 이 edge들을 계산한다([FramePipeline.hpp](../Engine/Render/Public/FramePipeline.hpp)).

## Bindless

셰이더 리소스는 **DescriptorHeap 인덱싱으로만** 접근한다.

- 셰이더는 push constant나 상수 버퍼 안에 `DescriptorHandle<Texture2D>` 같은 핸들을 들고 있다([Fullscreen.slang](../Engine/Shader/Fullscreen.slang)).
  CPU 쪽에서는 `texture->GetReadableID()`로 인덱스를 넣어준다. (SRV/UAV를 슬롯에 바인딩하는 게 아니다.)
- 샘플러는 [Sampler.slang](../Engine/Shader/Sampler.slang)에 정해둔 static sampler만 쓴다.
- 직접 바인딩되는 건 constant buffer 3개랑 push constant뿐이다.
  push constant 예산은 D3D12와 Metal 중 좁은 곳에 맞춘다. (`PushConstantBytes`)
  D3D12 루트 시그니처는 64 DWORD, CBV 하나가 2 DWORD이므로 `(64 - 2×3) × 4 = 232`Bytes다.
  Metal에 대응되는 `setBytes`는 4KB.
- Metal에선 Slang이 `DescriptorHandle<T>`를 argument buffer 안의 실제 텍스처 슬롯으로 처리하므로, 셰이더 리플렉션 코드에서 따로 처리한다([RHIShader.cpp](../Engine/RHI/Private/RHIShader.cpp)).

## Slang

HLSL와 MSL를 각각 유지하는 대신, Slang 하나를 사용한다.

- D3D12는 DXIL, Metal은 metallib으로 번역된다.
- 최소 셰이더 모델은 6.8이다. 모든 파이프라인이 `sm_6_8`(`ShaderProfile`)로 컴파일된다.
- 검색 경로:
  (1) 셰이더 파일 자기 폴더,
  (2) 공용 루트 [Engine/Shader](../Engine/Shader),
  (3) 각 모듈의 `Shader/` 폴더
- 셰이더 리플렉션으로 상수 버퍼 슬롯, compute thread group 크기, 사용하는 샘플러 정보를 얻는다.
  예상 못 한 파라미터 종류(바인딩된 SRV)는 예외를 던진다. (bindless 규칙 강제)
- 렌더러 샘플에서는 명령 포트의 `reload_shaders`로 실행 중에 다시 컴파일할 수 있다.
