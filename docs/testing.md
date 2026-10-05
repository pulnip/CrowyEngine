# Testing

What to run for a change is in `CLAUDE.md` (Verification). This file holds
the rules behind it: goldens, baselines, the stage, the other machine,
and what a test has to prove.

## Smoke runs and goldens

Every executable gets a `<Name>Smoke` test that runs it through
`Tools/smoke_run.ps1` (macOS: `smoke_run.sh`) for 5 s
(`CROWY_SMOKE_DURATION`). The run fails on a crash, an early nonzero exit,
or a D3D12 debug-layer error (the script sets `CROWY_D3D_DEBUG_BREAK=1`);
a window still open at the end passes. A check must exit by itself within
60 s, or it fails as hung. Exit 77 is a skip: the sample's content is
missing (`StageEditor` without `../Backlot`).

A golden is the frame-60 picture at
`Engine/*/{Sample,Spike}/Golden/<exe>.<dx12|metal>.png`. When one exists,
every run captures frame 60 and compares: to `%TEMP%\crowy-smoke\` by
default, or to `CROWY_SMOKE_CAPTURE_DIR` when set, which also keeps the
captures of samples without a golden. A slow start waits up to 60 s more
for frame 60, and a golden with no frame captured fails.

- The comparison is `ImageCompareCheck` with `ImageTolerance`'s defaults:
  each channel within 2, no pixel failing.
- A difference writes a heat map beside the capture and prints the
  `Copy-Item` (or `cp`) that accepts the new picture. Accept only a change
  you meant; say why in the commit.
- Re-recording one backend's golden makes the other backend's stale:
  delete it and list the sample for the other machine (below), or that
  machine fails for a reason that is not a bug.
- A golden of a scene still being art-directed is recorded once, when the
  look settles, not after every tweak.

## Tolerance-0 baselines

When a render change claims "the picture is unchanged" (a refactor, a new
pass that must be neutral), capture the affected samples before and after
at `ImageCompareCheck <a> <b> --tolerance 0` into `captures/` (git-ignored).
This is a manual step for that claim, not part of every run.

## The stage

`StageEditorCuts` (label `stage`) runs `Tools/stage_cuts.ps1`: every cut of
Backlot's scene in the pinned keys, against
`Engine/Render/Sample/Golden/StageEditor/*.<dx12|metal>.png`. The goldens are
stamped with a Backlot commit (`StageEditor.backlot.txt`); a Backlot at
another commit, or with changes to files the editor reads, is captured but
not compared (77). `-Record` re-records and re-stamps.
`Tools/stage_task.ps1` performs the owner's hand task over the port and
asserts each step; it is not in ctest.

## The other machine

Work is built and verified on one machine first; the other backend and the
other platform's scripts are written blind there. Whatever only the other
machine can verify (a blind backend path, its goldens, its scripts, a
platform-dependent number) goes into a short list file at the repo root,
added when an increment changes it and measured, never predicted. The list
says what to run and what passing looks like; the person on that machine
marks each line with the date and the result, and a failure becomes a task.

## What a test must prove

- A behavior the code could get wrong. A test that restates its own setup
  (asserts the loop bound it wrote, the table it just filled, a default it
  just set) proves nothing; delete it.
- A pinned number that came from running the code, not from reasoning
  (a hash, a tick count, a pixel count), says so in its name or a one-line
  comment, and is recorded by the tool that checks it, never typed by hand.
- Determinism pins stay apart from semantic tests, so a platform that
  drifts reads as drift, not as a broken mechanism.
- A fix for a bug class seen before adds the guard (a helper, a table, a
  test over every case) rather than one more point test.
- A headless check compares GPU results with a CPU reference and exits
  nonzero on a mismatch; printing the mismatch and returning 0 is a bug.
