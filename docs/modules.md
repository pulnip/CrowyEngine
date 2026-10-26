# Modules

What each module under `Engine/` owns, where to start reading it, and where
the things a change usually touches live. Paths below are under `Engine/`.

A module is `Engine/<Name>/`, declared by `crowy_declare_module`
(`cmake/util.cmake`) as `Crowy::<Name>`.
- `Public/` is on every dependent's include path, so a header is included
  by its bare name (`"StringUtil.hpp"`). `Private/` is the module's own; a
  few modules also export it as `Crowy::<Name>::Private`
  (`crowy_declare_private_interface`) for a test that needs it.
- `Test/` builds the module's `unit` binary, `Crowy<Name>Test`.
- `Sample/`, `Spike/` and `Check/` hold executables and sample libraries.
  A sample library is `Crowy::Sample<Name>`, declared by
  `crowy_declare_sample_library` (`cmake/sample.cmake`). All of them are
  built only with `CROWY_ENABLE_TEST`.
- `Shader/` holds the module's own Slang files; `Engine/Shader` is the
  shared root.

What a module links is the `PUBLIC` and `PRIVATE` lists in its
`CMakeLists.txt`; this file does not copy them.

## The modules

| Module | Owns | Start at | Samples and tests |
|---|---|---|---|
| `Core` | the type aliases, asserts, math and geometry, containers and handles, text and range helpers, `FixedTickClock` | `Primitives.hpp` (`u32`, `Str`, ...), `Assert.hpp` (`CROWY_ASSERT`), `StringUtil.hpp`, `RangeUtil.hpp`, `LinearAlgebra.hpp` | Test |
| `Log` | log levels, categories and sinks | `Log.hpp` | |
| `Platform` | the window, the loop (`OS` runs a `MainLoop`), input, the command line and runtime flags, the frame profiler | `OS.hpp`, `MainLoop.hpp`, `RuntimeConfig.hpp`, `InputProvider.hpp` | Test |
| `Reflection` | registered types and their properties, property paths and writes | `ClassRegistry.hpp` (`CROWY_STRUCT`), `PropertyTarget.hpp` | Test |
| `Serialization` | the DOM and JSON; image, mip, mesh and model (FBX) loading | `DOM.hpp`, `JsonLoader.hpp`, `ImageLoader.hpp`, `ModelLoader.hpp` | Test; Check: `ImageCompareCheck`, the picture comparer the goldens use |
| `Remote` | the debug command port, HTTP/JSON on 127.0.0.1, and its reflection verbs | `CommandPort.hpp` | Test; Sample: `HelloCommandPort`, a struct on the port end to end |
| `UI` | ImGui drawing and the reflection-driven inspector | `UIRenderer.hpp`, `PropertyWalker.hpp` | Test, Sample |
| `RHI` | the device, command lists, resources, barriers and shaders over D3D12 and Metal; `Common/` the shared definitions, `TestSupport/` the recording fakes | `RHIDevice.hpp`, `RHICommandList.hpp`, `Common/RHIDefinitions.hpp`, [rhi.md](rhi.md) | Test; Sample, which also holds `AppFramework` (`App`, `Main<T>`) that every windowed executable compiles; Spike; Check |
| `DX12RHI`, `MetalRHI` | the two backends, one per platform, chosen at build time | `Private/` only: `DX12Device.cpp`, `MetalDevice.cpp` | through RHI's |
| `Shader` | the shared Slang root: scene data, view, lighting, fullscreen | the `.slang` files | |
| `Render` | the render scene, the frame graph of passes, materials and pipelines, shadows, post, mesh assets | `SceneRenderer.hpp`, `RenderScene.hpp`, `FramePipeline.hpp`, `StandardPipeline.hpp` | Test; Sample: the renderer host (`RenderApp`, `FrameHost`), `Editor/`, `Stage/`, `PhysicsSync/` and the examples, `Playground` first; Spike |
| `Effects` | GPU fields, compute kernels and particle effects | `EffectSystem.hpp`, `ParticleEffects.hpp`, `FieldPass.hpp`, [writing-an-effect.md](writing-an-effect.md) | Test; Sample: `Island`; Check |
| `Physics` | Jolt behind a deterministic world | `PhysicsWorld.hpp`, `PhysicsRuntime.hpp` | Test; `Sample/Puzzle` |
| `World` | actors, components, systems, the fixed phases and clocks, the transform hierarchy, `ScriptComponent` | `World.hpp`, `Component.hpp` (`CROWY_COMPONENT`), `System.hpp` (`CROWY_SYSTEM`) | Test |
| `WorldRender` | mesh, light and camera components, extracted into a `RenderScene` and a `CameraSnapshot` | `MeshComponent.hpp`, `LightComponent.hpp`, `CameraComponent.hpp` | Test |
| `WorldEffects` | an actor's position written into an effect's origin | `EffectComponent.hpp` | Test |
| `Scene` | a World's actors saved to a scene file and loaded back | `Scene.hpp` | Test |
| `Asset` | asset names by mount and normalized path (`engine:` or the content root), each loaded once | `AssetRef.hpp`, `AssetStore.hpp` | Test |

`Engine/Content` is planned: the image, mip, mesh and model loaders move
there from Serialization, which keeps the DOM and JSON.

## What a module promises

- Render does not see the World. `WorldRender` and `WorldEffects` see both
  and copy the World's state across at Extract; `Scene` sees the World and
  Serialization.
- The World records what changed, and a frame where nothing did recomputes
  nothing. An actor's component or transform is a port and panel target
  found again at every access.
- A scene save leaves out a component marked `Transient`, and a component
  with no reflected properties stops the save.
- An `AssetStore` releases an asset at the `Collect` after its last handle
  goes. Render's `MeshStore` is one; `engine:Builtin/*` meshes come from
  code.
- Physics and the code that feeds it compile with `crowy_fp_contract_off`.

## Where is it

| Looking for | Where |
|---|---|
| A port verb | `RegisterVerb` calls in five files: `Remote/Private/CommandPort.cpp` (the reflection verbs, whose bodies are in `ReflectionVerbs.cpp`) and, in `Render/Sample/`, `FramePort.cpp` (`FramePort::RegisterVerbs`, the frame-loop verbs), `FrameHost.cpp`, `FrameCaptures.cpp`, `FrameStatsLog.cpp`. A running app's `GET /` lists every verb with its keys (`CommandPortImpl::info`); a key outside a verb's `VerbKeys` is refused. |
| A command-line flag | `applyCommandLine` in `Platform/Private/RuntimeConfig.cpp` parses them into `RuntimeConfig`, and its usage string lists them; the loop reads its own in `Platform/Private/OS.cpp`. |
| A struct on the port | `CROWY_STRUCT` (`Reflection/Public/ClassRegistry.hpp`) registers it, `CommandPort::Expose` puts an instance on the port; `Remote/Sample/HelloCommandPort.cpp` does both. |
| The other registrations | `CROWY_COMPONENT` in `World/Public/Component.hpp`, `CROWY_SYSTEM` in `World/Public/System.hpp`, `CROWY_ENUM_BEGIN` in `Core/Public/EnumUtil.hpp`. |
| Whether a held key counts while ImGui has the keyboard | `SDLInputProvider::IsKeyDown` checks the `InputGate` (`Platform/Private/SDLInputProvider.hpp`), which `OS::Impl::ProcessEvents` in `Platform/Private/OS.cpp` sets every frame from ImGui; a sample can lift it for a frame with `ImGui::SetNextFrameWantCaptureKeyboard(false)` (`Render/Sample/PlaygroundScene.cpp`). |
| Scene load and its errors | `loadScene` in `Scene/Private/Scene.cpp`, which gathers every error and throws them together. An unknown scene or actor key is `checkKeys` there; an unknown component field is `ApplyProperties` in `Reflection/Private/ClassRegistry.cpp`. |
| The windowed app | `App` and `Main<T>` in `RHI/Sample/AppFramework.hpp`; the renderer host in `Render/Sample/RenderApp.hpp` and `FrameHost.hpp`; the frame order in [architecture.md](architecture.md). |
| An executable or a test | `ctest -N`, with `-L <label>`, lists the entries; each is declared in its module's CMakeLists through `cmake/sample.cmake` or `crowy_declare_test` (`cmake/util.cmake`). |
