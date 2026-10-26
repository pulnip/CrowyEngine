# CrowyEngine

This file is the one source for how to build, run, verify and commit here.

## Repository

C++23 under clang-cl (Windows) and Homebrew LLVM (macOS), shaders in Slang,
backends D3D12 and Metal. Modules under `Engine/`:

- `Core` (math, containers, `FixedTickClock`), `Log`, `Platform` (window,
  input, `RuntimeConfig` flags), `Reflection`, `Serialization` (JSON, FBX
  `loadModel`), `Remote` (the command port), `UI` (ImGui panels).
- `RHI` with `DX12RHI` / `MetalRHI`; `Render` (scene, pipeline, passes);
  `Shader` (the shared Slang root); `Effects` (GPU particles and fields);
  `Physics` (Jolt; its targets and glue compile with
  `crowy_fp_contract_off`).
- `World` (actors, component pools, the fixed phases and clocks, the
  transform hierarchy, `ScriptComponent` for one actor's own behaviour; it
  records what changed, and a frame where nothing did recomputes nothing;
  an actor's component or transform is a port and panel target found again
  at every access), `WorldRender` (the World's meshes, lights and camera
  extracted into a `RenderScene` and a `CameraSnapshot`; Render does not
  see the World) and `WorldEffects` (an actor's position written into an
  effect's origin). `Scene` saves a World's actors to a scene file and
  loads one back; a component marked `Transient` is left out, and one with
  no reflected properties stops the save. `Asset` names an asset by its
  mount and normalized path (`AssetRef`, `engine:` or the content root) and
  loads each once, released at the `Collect` after its last handle goes
  (`AssetStore`); nothing loads through it yet.

Executables are Examples, `*Spike` (a picture pins a backend difference) or
`*Check` (headless, exits nonzero on a wrong number), declared in
`cmake/sample.cmake`. ctest labels: `unit`, `gpu` (checks), `render`
(spikes), `app` (four hosts), `scenario`, `content` (`docs/testing.md`); an
executable without one is built only.

## Code conventions here

These override the global CLAUDE.md where they differ.

- C++23. Use the `Core/Primitives.hpp` aliases (`u32`, `usize`, `f32`, ...),
  not `std::uint32_t`. Everything lives in `namespace Crowy`.
- Code text spells "color". When a test matches a message, grep both sides
  after rewording it.
- Comments: the default is none. Write one only for a fact the code, its
  names, its asserts and its tests cannot show: an external API's or
  driver's behaviour, a spec number, a decision whose alternative is not
  visible from here. One line, two at most. Never what a test shows, what
  the next line does, a plan, a date or a step. A protocol or format
  someone outside the repo reads goes in docs/, not in a header.
- Format: a module listed in Tools/formatted.txt is formatted whole
  (`clang-format -i`), and `Tools/format_check.ps1` fails on drift; a
  new file is formatted whole and its module added to the list. Elsewhere
  `git clang-format` on changed lines only.
- Shared text and range helpers live in Core: StringUtil.hpp (join,
  parseNumber, case), PathUtil.hpp, FileUtil.hpp, RangeUtil.hpp (stray,
  firstRepeat). Look there before writing a loop.
- Anything that advances per frame is paced by time (the owner's display is
  120 Hz); goldens stay deterministic by counted frames.
- A registration (`CROWY_STRUCT`, `CROWY_COMPONENT`, `CROWY_ENUM_BEGIN`,
  ...) sits between `// clang-format off` and `// clang-format on`.
- A file that only registers (`CROWY_STRUCT`, `CROWY_COMPONENT`) goes in a
  static library wrapped by `crowy_whole_archive` (`cmake/util.cmake`); a
  plain one drops the object and the type silently goes missing.
- Read `Engine/RHI/Sample` and its `.slang` for RHI usage, and
  `Engine/Render/Sample` (`RenderApp`, `Playground`) for renderer work,
  before reading wider.

## Building

On Windows, build only through `Tools/build.ps1` (macOS: `Tools/build.sh`,
same flags in sh style). It imports the VS environment and configures when
the cache is missing; a hand-written `cmake` or `ninja` has neither.

```bash
powershell -NoProfile -File Tools/build.ps1 -Config Debug -Target CrowyRenderTest -Detach
powershell -NoProfile -File Tools/build.ps1 -Status
```

- Poll until `running : False`, then read `build/build-log.txt`. A tool
  timeout is not a failure; the detached build continues.
- A full Debug build is ~60 s, an incremental one ~15 s; editing
  `RenderApp.cpp` rebuilds every executable that compiles it. Ten minutes
  means wedged: check `Get-Process ninja,clang-cl,lld-link`, never kill it.
- One target per call, one build at a time. Windows PowerShell 5.1
  (`powershell`), never `pwsh`.
- `file(GLOB)` has no `CONFIGURE_DEPENDS`: a new source file needs a
  reconfigure (editing any `CMakeLists.txt` triggers one).
- The bench tree is `Tools/build_bench.ps1`, Release; its first build
  passes `-Fresh`.

## Running

- Run from the repo root (samples load `Engine/Shader` and `Content` by
  relative path). Set `CROWY_WINDOW_DISPLAY=1` for every window you open.
- `Tools/smoke_run.ps1 <exe>` runs one executable the way ctest does.
- The command port (Debug only) listens on 27500; `StageEditor`'s scripts
  use 27520, `Tools/input_checks.ps1` 27530; `CROWY_COMMAND_PORT` overrides
  (0 disables). A second worktree
  running at the same time needs its own port. `--hold` starts paused.
  `--capture-frame <frame> <path>` (any build, repeatable) writes that
  frame as a BMP and runs every frame as a fixed step; the spikes' goldens
  are frame 60 captured this way. `--exit-after-frame <frame>` quits after
  presenting that frame; every windowed ctest entry ends this way.
  `--port-input` hears no keyboard or mouse and takes the port's
  `push_input` instead (`read_input` reports what the app saw); its window
  never takes the focus, so a script never moves the owner's cursor.
  Drive it with `. Tools/port.ps1` (`Wait-Port`, `Set-PortProperty`,
  `Get-PortProperty`, `Save-PortFrame`, `Invoke-Port quit`).
- On Windows, read `docs/windows.md` before debugging a crash, a silent
  D3D12 error or a rebuild that ignored an edit.

## Verification

Run a test for a reason: to diagnose a failure, to confirm an increment,
or for a merge.

- Diagnosing: rerun only what failed (`ctest -R "^<name>$"`,
  `--gtest_filter`).
- Iterating: build the target and run its own unit binary; never `gpu`,
  `render` or `app` while the logic is still changing.
- Confirming an increment, once before its commit: the `unit` label,
  `Tools/format_check.ps1`, plus the row below for what changed.
- A merge to master, when the owner asks: the whole ctest, once.

| Changed | Add, once per increment |
|---|---|
| RHI, DX12RHI, MetalRHI, `Engine/Shader` | `-L "gpu\|render"` |
| Render | `-L render`, `-R "^Playground$"` |
| Effects GPU code or `.slang` | `-L gpu`, `-R "^Island$"` |
| Physics, the puzzles, PhysicsSync | `-L scenario`, `-R "^PhysicsPlayground$"` |
| The window path (OS, AppFramework, swapchain, RenderApp, UIRenderer, port verbs), a reflection registration, a CMake link | `-L app` |
| Platform input, the OS event path | `-L app`, `Tools/input_checks.ps1` |
| The Stage loader, a Backlot bump | `-L content`, `Tools/stage_cuts.ps1` |
| A `Tools/` script | that script, by hand |
| Anything else: CPU modules, tests, docs | nothing more |

```bash
ctest --test-dir build -C Debug -L unit -j 8
ctest --test-dir build -C Debug -L "gpu|render" -j 8
ctest --test-dir build -C Debug -j 8
```

- Do not run: a passing test again on an unchanged binary; the whole ctest
  outside a merge; a by-hand debug-layer, GBV or two-run determinism pass;
  anything to count tests. The final message names the row run and its
  result.
- Before adding a test, an executable, a golden or a ctest entry, read
  `docs/testing.md` (Adding a test).
- `-j` runs headless entries side by side; windowed ones stay one at a
  time (`RUN_SERIAL`). A unit binary with no tests fails by itself.
- Windowed runs break on D3D12 debug-layer errors, and a spike compares
  its golden on every run; there is no separate "debug layer" pass.
  Goldens, baselines and cross-machine checks: `docs/testing.md`.
- Fix only review findings that survived an independent refutation, one
  defect per commit; style goes through clang-format, not a review fix.

## Git

- Commit only when the owner asks; never push, amend, rebase or reset.
- Commit message: the subject alone, one short imperative clause,
  lowercase. A body only when the diff would look wrong without it, two
  lines at most. No test counts, no file lists. End with the session's
  Co-Authored-By line.
