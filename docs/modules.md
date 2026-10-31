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
| `Core` | the type aliases, asserts, math and geometry, containers and handles, text, path, file and range helpers, `FixedTickClock` | `Primitives.hpp` (`u32`, `Str`, ...), `Assert.hpp` (`CROWY_ASSERT`), `StringUtil.hpp`, `PathUtil.hpp`, `FileUtil.hpp`, `RangeUtil.hpp`, `LinearAlgebra.hpp` | Test |
| `Log` | log levels, categories and sinks | `Log.hpp` | |
| `Platform` | the window, the loop (`OS` runs a `MainLoop`), input, the command line and runtime flags, the frame profiler | `OS.hpp`, `MainLoop.hpp`, `RuntimeConfig.hpp`, `InputProvider.hpp` | Test |
| `Reflection` | registered types and their properties, property paths and writes | `ClassRegistry.hpp` (`CROWY_STRUCT`), `PropertyTarget.hpp` | Test |
| `Serialization` | the DOM and JSON | `DOM.hpp`, `JsonLoader.hpp` | Test |
| `Remote` | the debug command port, HTTP/JSON on 127.0.0.1, and its reflection verbs | `CommandPort.hpp` | Test; Sample: `HelloCommandPort`, a struct on the port end to end |
| `UI` | ImGui drawing and the reflection-driven inspector | `UIRenderer.hpp`, `PropertyWalker.hpp` | Test, Sample |
| `RHI` | the device, command lists, resources, barriers and shaders over D3D12 and Metal; `Common/` the shared definitions, `TestSupport/` the recording fakes | `RHIDevice.hpp`, `RHICommandList.hpp`, `Common/RHIDefinitions.hpp`, [rhi.md](rhi.md) | Test; Sample, which also holds `AppFramework` (`App`, `Main<T>`) that every windowed executable compiles; Spike; Check |
| `DX12RHI`, `MetalRHI` | the two backends, one per platform, chosen at build time | `Private/` only: `DX12Device.cpp`, `MetalDevice.cpp` | through RHI's |
| `Shader` | the shared Slang root: scene data, view, lighting, fullscreen | the `.slang` files | |
| `Render` | the render scene, the frame graph of passes, materials and pipelines, shadows, post, mesh and material assets, material files (`MaterialDesc`) | `SceneRenderer.hpp`, `RenderScene.hpp`, `FrameGraph.hpp`, `StandardFrame.hpp` | Test; Sample: the renderer host (`RenderApp`, `FrameHost`), `Editor/` (StageEditor's session, frozen until the real editor replaces it), `Stage/`, `PhysicsSync/` and the examples, `Playground` first, which opens the scene file `Playground/Playground.json` of built-in meshes and material files; Spike |
| `Effects` | GPU fields, compute kernels and particle effects | `EffectSystem.hpp`, `ParticleEffects.hpp`, `FieldPass.hpp`, [writing-an-effect.md](writing-an-effect.md) | Test; Sample: `Island`; Check |
| `Physics` | Jolt behind a deterministic world | `PhysicsWorld.hpp`, `PhysicsRuntime.hpp` | Test; `Sample/Puzzle` |
| `World` | actors, components, systems, the fixed phases and clocks, the transform hierarchy, `ScriptComponent` | `World.hpp`, `Component.hpp` (`CROWY_COMPONENT`), `System.hpp` (`CROWY_SYSTEM`) | Test |
| `WorldRender` | mesh, light, camera and environment components, extracted into a `RenderScene` and a `CameraSnapshot`; `EnvironmentComponent` (one per World, its sun a `LightComponent` on the same actor) holds the ambient and the sky colour the frame clears to; a World may hold many cameras and the lowest `FileId` is the one seen; `MeshComponent` names its assets, `MeshRowComponent` draws a row the code built; `PrimitiveActors`, when the World provides it, names the actor each row draws | `MeshComponent.hpp`, `MeshRowComponent.hpp`, `LightComponent.hpp`, `CameraComponent.hpp`, `EnvironmentComponent.hpp`, `PrimitiveActors.hpp` | Test |
| `WorldEffects` | an actor's position written into an effect's origin | `EffectComponent.hpp` | Test |
| `Scene` | a World's actors saved to a scene file and loaded back | `Scene.hpp` | Test; Sample: `HelloScene`, a scene file of built-in meshes, two material files and its own environment opened and drawn; `Backlot/` (`BacklotImport.hpp`), the headless `ImportBacklot`, which reads Backlot's `scene.json` through Stage and writes the default lighting key as a scene file, material files, the models and images they name and a copy of the source scene under one content folder (`--backlot`, `--out`; default `Content/Backlot`), the only code in Scene that sees the Stage reader; Test, and a `content` test of the real set; Spike: `BacklotSceneSpike`, the imported `Content/Backlot/Backlot.json` opened and drawn |
| `Editor` | editing a World: the session that selects actors by `ActorId` and moves them, the camera outside the World, picking against a `RenderScene`, the gizmo's layout, hover and drag | `WorldEditor.hpp`, `EulerInspector.hpp`, `EditorGizmo.hpp`, `EditorPick.hpp`, `EditorCamera.hpp` | Test |
| `Asset` | asset names by mount and normalized path (`engine:` or the content root), each loaded once | `AssetRef.hpp`, `AssetStore.hpp` | Test |
| `Content` | image, mip, mesh and model (FBX) loading, image writing and comparison | `ImageLoader.hpp`, `MeshData.hpp`, `ModelLoader.hpp`, `ImageCompare.hpp` | Test; Check: `ImageCompareCheck`, the picture comparer the goldens use |

## What a module promises

- Render does not see the World. `WorldRender` and `WorldEffects` see both
  and copy the World's state across at Extract; `Scene` sees the World and
  Serialization.
- A scene file carries its environment: `EnvironmentComponent`'s sky colour
  clears the lit views of the frame, and a World without one leaves the
  clear and `RenderScene::Environment()` as the code set them. A camera
  other than the lowest `FileId` writes no `CameraSnapshot`, and the
  snapshot does not follow a hand-over when the camera seen is destroyed.
- The World records what changed, and a frame where nothing did recomputes
  nothing. An actor's component or transform is a port and panel target
  found again at every access.
- A scene save leaves out a component marked `Transient`, and a component
  with no reflected properties stops the save.
- An `AssetStore` releases an asset at the `Collect` after its last handle
  goes. Render's `MeshStore`, `TextureStore` and `MaterialStore` are three;
  `engine:Builtin/*` meshes (`Box`, `Sphere`, `Plane` facing +Y, `Quad`
  facing -Z, `Missing`), textures (`White`, `FlatNormal`, `Missing`) and
  materials (`Opaque`, `Translucent`, `Masked`, `Missing`) come from code,
  and any other material ref is a JSON file of one `MaterialDesc`.
- A texture is one asset per path, always with mips (a PNG gets the CPU
  chain, a KTX2 keeps its own). The material's slot (`albedoMap`,
  `emissiveMap`) names the sampler and reads the texture as sRGB; a nearest
  sampler reads one mip. A texture ref that fails to load is told and
  sampled as `engine:Builtin/Missing`, and the file keeps the path. A
  material holds its textures, so `MaterialStore` collects before
  `TextureStore`.
- Physics and the code that feeds it compile with `crowy_fp_contract_off`.

## Where is it

| Looking for | Where |
|---|---|
| A port verb | `RegisterVerb` calls in five files: `Remote/Private/CommandPort.cpp` (the reflection verbs, whose bodies are in `ReflectionVerbs.cpp`) and, in `Render/Sample/`, `FramePort.cpp` (`FramePort::RegisterVerbs`, the frame-loop verbs), `FrameHost.cpp`, `FrameCaptures.cpp`, `FrameStatsLog.cpp`. A running app's `GET /` lists every verb with its keys (`CommandPortImpl::listVerbs`); a key outside a verb's `VerbKeys` is refused. |
| A command-line flag | `applyCommandLine` in `Platform/Private/RuntimeConfig.cpp` parses them into `RuntimeConfig`, and its usage string lists them; the loop reads its own in `Platform/Private/OS.cpp`. |
| A struct on the port | `CROWY_STRUCT` (`Reflection/Public/ClassRegistry.hpp`) registers it, `CommandPort::Expose` puts an instance on the port; `Remote/Sample/HelloCommandPort.cpp` does both. |
| The other registrations | `CROWY_COMPONENT` in `World/Public/Component.hpp`, `CROWY_SYSTEM` in `World/Public/System.hpp`, `CROWY_ENUM_BEGIN` in `Core/Public/EnumUtil.hpp`. |
| Whether a held key counts while ImGui has the keyboard | `SDLInputProvider::IsKeyDown` checks the `InputGate` (`Platform/Private/SDLInputProvider.hpp`), which `OS::Impl::ProcessEvents` in `Platform/Private/OS.cpp` sets every frame from ImGui; a sample can lift it for a frame with `ImGui::SetNextFrameWantCaptureKeyboard(false)` (`Render/Sample/PlaygroundScene.cpp`). |
| Scene load and its errors | `loadScene` in `Scene/Private/Scene.cpp`, which gathers every error and throws them together. An unknown scene or actor key is `checkKeys` there; an unknown component field is `applyProperties` in `Reflection/Private/ClassRegistry.cpp`. |
| The windowed app | `App` and `Main<T>` in `RHI/Sample/AppFramework.hpp`; the renderer host in `Render/Sample/RenderApp.hpp` and `FrameHost.hpp`; the frame order in [architecture.md](architecture.md). |
| An executable or a test | `ctest -N`, with `-L <label>`, lists the entries; each is declared in its module's CMakeLists through `cmake/sample.cmake` or `crowy_declare_test` (`cmake/util.cmake`). |
