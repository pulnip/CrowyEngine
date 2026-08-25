# CrowyEngine

## Building

On Windows, build only through `Tools/build.ps1`. It imports the VS developer
environment and configures when the cache is missing; a hand-written `cmake` or
`ninja` command has neither, and bare `ninja` additionally resets `.ninja_log`
into a full rebuild.

```bash
powershell -NoProfile -File Tools/build.ps1 -Config Debug -Target CrowySceneTest -Detach
```

`-Detach` survives tool timeouts. Poll it until `running : False`:

```bash
powershell -NoProfile -File Tools/build.ps1 -Status
```

- Windows PowerShell 5.1 (`powershell`) is the baseline — the same host
  `Engine/RHI/Sample/CMakeLists.txt` runs the smoke tests with, and every script
  under `Tools/` stays inside what it parses. Do not write `pwsh`; PowerShell 7
  is not assumed to be installed.
- One target per call. `-Target A,B` reaches ninja as one name and fails.
- One build at a time, and never kill one — orphans hold `.ninja_lock` and force
  the next build into a full rebuild.
- A tool timeout is not a failure. The detached build is still going, so poll
  instead of restarting.
- A full Debug build is ~60 s. Ten minutes means wedged, not slow.
- Read the log after the build ends. Piping a running one makes it look hung.

`file(GLOB)` has no `CONFIGURE_DEPENDS`, so a new source file needs a
reconfigure. Editing any `CMakeLists.txt` triggers one.

## Testing

```bash
ctest --test-dir build -C Debug -L unit
```

Check how many tests ran, not just the exit code — gtest exits 0 with zero tests
registered, so a linkage problem reads as a pass.

## Smoke Running

```bash
powershell -NoProfile -File Tools/smoke_run.ps1 <exe>
```

From the repo root.

`CROWY_SMOKE_CAPTURE_DIR` plus `CROWY_DUMP_FRAME` dumps a frame BMP (32bpp BGRA, bottom-up) at presented frame 60.

## Guideline

- Read `Engine/RHI/Sample` and its `.slang` files before looking wider — those are
  the canonical RHI usage patterns.
