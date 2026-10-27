# CrowyEngine

D3D12와 Metal 위에서 동작하는, C++23 기반의 렌더링/시뮬레이션 엔진이다.

셰이더는 Slang을 사용한다.

## 목적

게임 하나 보다는, 내 손에 맞는 렌더링/시뮬레이션 실험실을 만드는 것이 주 목적이다.

![슈바르츠실트 블랙홀 주변의 광선을 측지선으로 적분해서 그린 강착 원반](docs/images/blackhole.gif)

*BlackholeSimulation: 슈바르츠실트 블랙홀 주변의 빛 경로를 RK4로 수치 적분한다. 도플러 효과와 중력 적색편이를 넣고, HDR 타깃에 그린 뒤 mip 체인 bloom을 거친다.*

## 빌드 & 실행

```bash
git clone https://github.com/pulnip/CrowyEngine.git
cd CrowyEngine
git lfs pull
```

빌드는 [CMakePresets.json](CMakePresets.json)의 preset으로 한다.
`Tools/build.ps1` / `Tools/build.sh`도 같은 preset으로 빌드하고, 환경을 불러오고 컴파일러를 점검하는 일까지 대신 해 주므로 개발 중에는 이쪽이 편하다.

서드파티(SDL3, Dear ImGui, Slang, Jolt, GoogleTest 등)는 첫 configure 때 FetchContent로 받는다. (네트워크 필요)

블랙홀 샘플은 LFS 없이도 실행 가능하므로, 블랙홀 샘플을 기준으로 설명한다. golden 테스트, UI 폰트, 샘플 에셋은 LFS가 필요하다.

### Windows

- x64, Shader Model 6.8 이상
- LLVM clang-cl **20 이상** (VS 2026의 STL이 요구)
- Visual Studio C++ 도구 + Windows SDK, CMake **3.31 이상**, Ninja, git-lfs
- 개발 환경: LLVM 22.1.8, VS 2026 18.10, CMake 4.4, Windows SDK 10.0.26100

깊은 폴더에 clone하면 받은 파일 경로가 260자 제한에 걸려서 configure가 실패할 수 있다.
짧은 경로에 clone하거나 `git config --global core.longpaths true`를 켜 두면 된다.

아래 명령은 **Developer PowerShell for VS**(x64)에서 실행한다.
preset이 VS 환경을 직접 불러오지 않으므로, 일반 셸에서는 clang-cl이 STL과 Windows SDK를 찾지 못한다.

```bash
cmake --preset Windows-llvm-x64
```

```bash
cmake --build --preset Windows-llvm-x64-Debug --target BlackholeSimulation
```

```bash
build\bin\Debug\BlackholeSimulation.exe
```

### macOS

- **Apple Silicon** (Slang이 `macos-aarch64` 바이너리만 제공)
- Homebrew LLVM
- Xcode Command Line Tools, CMake 3.31 이상, Ninja, git-lfs
- 개발 환경: Homebrew clang 23.1.3, CMake 4.4.4

`brew`가 PATH에 있어야 한다.
툴체인 파일이 brew로 LLVM 위치를 찾는데, 찾지 못하면 에러 없이 AppleClang으로 configure된다.

```bash
cmake --preset macOS-brew-llvm
```

```bash
cmake --build --preset macOS-brew-llvm-Debug --target BlackholeSimulation
```

```bash
build/bin/Debug/BlackholeSimulation
```

### 테스트

```bash
ctest --test-dir build -C Debug -L unit -j 8
```

영화 세트 샘플(`StageEditor`)은 비공개 콘텐츠 repo `Backlot`가 옆에 있어야 한다.
없으면 그 샘플과 `content` 테스트만 skip되고(종료 코드 77), 빌드와 나머지는 그대로 돈다.

## 모듈 구성

| 모듈 | 내용 |
|---|---|
| `Core`, `Log` | 수학, 컨테이너, `FixedTickClock`, 로그 |
| `Platform` | 창, 입력, 메인 루프(`OS` / `MainLoop`), 커맨드라인 설정 |
| `RHI`, `DX12RHI`, `MetalRHI` | 그래픽스 API 추상화와 두 백엔드 |
| `Shader` | 공용 Slang 루트 |
| `Render` | 렌더 씬, 패스 파이프라인, 그림자, 톤매핑 |
| `Reflection`, `Serialization` | 리플렉션 등록, DOM과 JSON |
| `Content` | FBX/KTX2/이미지 로딩, 이미지 쓰기와 비교 |
| `Remote`, `UI` | HTTP 명령 포트, ImGui 패널과 리플렉션 기반 인스펙터 |
| `World`, `WorldRender`, `WorldEffects` | 액터와 컴포넌트, 고정 phase, transform 계층. 렌더 씬과 이펙트로 내보내기 |
| `Effects`, `Physics` | GPU 파티클·필드, Jolt 기반 결정적 물리 |
| `Scene`, `Asset` | 씬 파일 저장/로드, 에셋 이름과 수명 관리 |

## 하이라이트

- **[RHI](docs/rhi.md)**: Bindless와 Enhanced Barrier 등 주요 RHI 설계
- **[프레임 구조](docs/architecture.md)**: 실행 흐름 설계. 창과 프레임 페이싱은 `OS`가, 앱의 동작은 `MainLoop`로 분리.
- **[검증](docs/verification.md)**: AI가 만든 코드를 어떻게 검증할 것인가에 대한 설계

## AI와 작업한 방식

이 엔진은 Claude와 페어 프로그래밍으로 만들었다.
AI가 먼저 patch로 구현하고, 그걸 읽으면서 모르는 부분을 물어보고 이해한 단위로 커밋했다.
설계 방향과 무엇을 받아들일지는 직접 정했다.

1. AI와의 작업 편의성을 우선하여, (1) json을 사용하고 (2) 텍스트 기반 씬 파일을 쓰고 (3) 런타임에 값 변경을 위해 명령 포트를 만들었다.
2. 동시에, AI의 실수를 어떻게 [검증](docs/verification.md)할지를 매우 신경썼다.
3. 추가적으로, 그 구조 위해서 Claude 스스로 새 기능을 얼마나 혼자 잘 만드는지 시험했다. (Effects의 `Island`, `PhysicsPlayground`)

## 한계

- 실험실이 목적이라서 게임을 만드는 데 필요한 것(에셋 파이프라인, 에디터 완성도, 패키징)은 우선순위가 밀렸다.
- 아직 메인 루프는 단일 스레드다. 이벤트 처리, 업데이트, 커맨드 기록이 한 스레드에서 돈다.

## License

[MIT](LICENSE)
