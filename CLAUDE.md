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
- `Resource` and `Scene` have no consumers yet.

Executables are Examples, `*Spike` (a picture pins a backend difference) or
`*Check` (headless, exits nonzero on a wrong number), declared in
`cmake/sample.cmake`. ctest labels: `unit`, `smoke` (every executable),
`example`, `spike`, `check`, `stage`.

## Code conventions here

These override the global CLAUDE.md where they differ.

- C++23. Use the `Core/Primitives.hpp` aliases (`u32`, `usize`, `f32`, ...),
  not `std::uint32_t`. Everything lives in `namespace Crowy`.
- Code text spells "color". When a test matches a message, grep both sides
  after rewording it.
- A comment is one line, two at most, and says why. No process notes, no
  milestone or ticket names, no history of a bug.
- Format: `clang-format -i` on a file you create; `git clang-format` on the
  lines you change in an older file, never the whole file.
- Anything that advances per frame is paced by time (the owner's display is
  120 Hz); goldens stay deterministic by counted frames.
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
  use 27520; `CROWY_COMMAND_PORT` overrides (0 disables). A second worktree
  running at the same time needs its own port. `--hold` starts paused.
  Drive it with `. Tools/port.ps1` (`Wait-Port`, `Set-PortProperty`,
  `Get-PortProperty`, `Save-PortFrame`, `Invoke-Port quit`).
- On Windows, read `docs/windows.md` before debugging a crash, a silent
  D3D12 error or a rebuild that ignored an edit.

## Verification

Run what the change can break, not everything:

| Changed | Run |
|---|---|
| Any code | build its targets, then the `unit` label |
| Render, Shader or RHI code | + the golden smokes below |
| Effects | + the `check` label and `IslandSmoke` |
| Physics | + `PhysicsPlaygroundSmoke` |
| A `Tools/` script | that script, by hand |
| Before a merge to master | the whole `smoke` label (39 tests, ~4 min) |

```bash
ctest --test-dir build -C Debug -L unit
ctest --test-dir build -C Debug -R "Playground|ShadowSample|PrepassEqual|Textured|Island|StageEditor"
```

- A unit binary with no tests fails by itself; do not count tests.
- Smoke runs break on D3D12 debug-layer errors and compare a golden on
  every run where one exists; there is no separate "debug layer" pass.
  Goldens, baselines and cross-machine checks: `docs/testing.md`.
- Fix only review findings that survived an independent refutation, one
  defect per commit; style goes through clang-format, not a review fix.

## Git

- Commit only when the owner asks; never push, amend, rebase or reset.
- Message: one short imperative clause, lowercase, like `drop the frame-slot
  machinery from buffer creation`. A body only when the why is not obvious,
  a few lines. End with the session's Co-Authored-By line.
