# Windows traps

- Restoring a file with `Copy-Item` keeps its old mtime and ninja skips it:
  edit it back or touch it, then grep `build/build-log.txt` for its compile
  line before trusting the binary.
- `powershell -File script.ps1 -Target A,B` passes one string `A,B`.
- A smoke run ends a windowed sample through its own shutdown
  (`--exit-after-frame`); only a hang or a headless example's timed run
  ends with `Stop-Process`, which hides a crash in teardown.
- D3D12 debug-layer messages reach stderr only with
  `CROWY_D3D_DEBUG_BREAK=1`; without it a hand run prints nothing.
- lldb is broken (no `python311.dll`) and there is no cdb or WinDbg. For a
  crash, read the Application event log (Id 1000) for the faulting module
  and offset, then
  `llvm-symbolizer --obj=build\bin\X.exe --inlines --relative-address 0x<offset>`.
