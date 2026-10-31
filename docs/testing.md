# Testing

What to run for a change is in `CLAUDE.md` (Verification). This file holds
the rules behind it: the kinds of test, when one is added, goldens,
baselines, the stage, the other machine, and what a test has to prove.

## Kinds

A test has one ctest label, and the label says what it needs. Which labels
a change adds is the Verification table in `CLAUDE.md`.

| Label | Needs | Holds |
|---|---|---|
| `unit` | CPU only: no device, window or file outside the repo; a sleep only where elapsed time is the contract (a timer, a timeout) | contracts of functions, classes and RHI calls on the fakes; reference models and seeded fuzz |
| `gpu` | a device, no window | `*Check`: device results held against a CPU reference |
| `render` | a window | `*Spike`: one backend semantic pinned by its frame-60 golden |
| `app` | a window and content | a host starts, runs its frames and shuts down by itself, with no assert or debug-layer error |
| `scenario` | CPU, long simulations | sample content behaves: the puzzles solve, their pinned hashes hold |
| `content` | `../Backlot`, or `Content/Backlot` alone | the external set imports at its pinned commit, skipped without it; the imported set against its source copy, which needs no checkout |

Outside ctest:
- An example whose declaration names no `TEST` is built only.
  `Tools/smoke_run.ps1 <exe>` runs one when you want to look at it.
- Tolerance-0 baselines (below), benchmarks (`Tools/bench_run.ps1`), and
  the stage scripts.

Words used here:
- **Smoke** run: one executable through `Tools/smoke_run`, the way its
  ctest entry runs it (any `gpu`, `render` or `app` entry; a spike's run
  also compares its golden).
- **Spike** means an experiment. One that is kept pins its answer as a
  `render` golden.
- **Check** means a component test on the device: `gpu`.
- **Golden** is a way to judge a test, not a kind of test.

## Adding a test

A test is added for a kind of defect, not for a function, a feature or a
round of work.

1. Name the wrong behavior it catches and why it can happen: a bug seen, a
   review finding, a backend difference. New code, coverage and a test
   count are not reasons.
2. Search the existing tests for that behavior first. If one would fail on
   the defect, add nothing; if one nearly would, extend it (a row, a
   parameter, a seed).
3. Take the cheapest kind that can see it, in this order:
   - an assert or `static_assert` in the code;
   - `unit`;
   - `gpu`;
   - `render`;
   - `app`.

   A new executable, ctest entry or golden needs the owner's yes and a
   one-line reason above its declaration.
4. Show it failing once against the defect; do not rerun that by hand.
5. A test that covers another deletes it in the same commit.

A fix adds no test when:
- an existing test failed on the defect;
- the check could only restate the fix (the same formula, its own setup);
- the change is look or tuning (a golden or pin is recorded once, when the
  owner accepts it);
- the finding is a name, a comment or style.

A design or task file may name the defects a round must guard. It does not
prescribe tests, counts or gates.

## Windowed runs and goldens

An executable whose declaration in its module's CMakeLists names
`TEST gpu`, `TEST render` or `TEST app` gets a ctest entry of its own name,
which runs it through `Tools/smoke_run.ps1` (macOS: `smoke_run.sh`).
- A windowed one gets `--exit-after-frame` and must exit by itself,
  shutdown included, within 60 s (`CROWY_SMOKE_TIMEOUT`).
- It runs 60 frames, or the `FRAMES` its declaration gives, or
  `CROWY_SMOKE_FRAMES`.
- A check gets no arguments and must also exit within 60 s.
- The run fails on a crash, a nonzero exit, a hang, or a D3D12 debug-layer
  error (the script sets `CROWY_D3D_DEBUG_BREAK=1`).
- Exit 77 is a skip: the program's content is missing (`StageEditor`
  without `../Backlot`).

By hand, `Tools/smoke_run.ps1 <exe> [capture-path]` runs it the same way.
- A capture path writes one frame there as a BMP; without one,
  `CROWY_SMOKE_CAPTURE_DIR` collects `<exe>.bmp` per run.
- The frame is 60 unless `CROWY_SMOKE_CAPTURE_AT` names another, and the
  run lasts until it. A golden is compared only at frame 60.
- A `.bmp` capture becomes a PNG through `ImageCompareCheck` beside the
  executable; the BMP stays when that tool is not built.
- `CROWY_SMOKE_MUST_EXIT=1`, which a check's entry sets, passes no
  arguments.

A golden is a spike's frame-60 picture at
`Engine/*/Spike/Golden/<exe>.<dx12|metal>.png`.
- When one exists, every run captures frame 60 (the app's
  `--capture-frame`) and compares it. The capture goes to
  `%TEMP%\crowy-smoke\` by default, or to `CROWY_SMOKE_CAPTURE_DIR` when
  set.
- A run that exited has written its captures, and a golden with no frame
  captured fails.
- The comparison is `ImageCompareCheck` with `ImageTolerance`'s defaults:
  each channel within 2, no pixel failing.
- A difference writes a heat map beside the capture and prints the
  `Copy-Item` (or `cp`) that accepts the new picture. Accept only a change
  you meant; say why in the commit.
- Re-recording one backend's golden makes the other backend's stale:
  delete it and list the spike for the other machine (below), or that
  machine fails for a reason that is not a bug.

A sample's picture under `Engine/*/Sample/Golden/` is a baseline, not a
golden. ctest never compares it. It is the "before" of a tolerance-0
comparison, and the stage scripts' launch picture. It is re-recorded only
when the owner accepts a new look, once, not after every tweak.

## Tolerance-0 baselines

When a render change claims "the picture is unchanged" (a refactor, a new
pass that must be neutral), capture the affected samples and compare them
with their baseline, or with captures from before the change:
`ImageCompareCheck <a> <b> --tolerance 0`, into `captures/` (git-ignored).
This is a manual step for that claim, not part of every run.

## The stage

`Tools/stage_cuts.ps1` checks every cut of Backlot's scene in the pinned
keys against `Engine/Render/Sample/Golden/StageEditor/*.<dx12|metal>.png`.
Run it by hand when the Stage loader or Backlot changes.
- The pictures are stamped with a Backlot commit
  (`StageEditor.backlot.txt`).
- A Backlot at another commit, or with changes to files the editor reads,
  is captured but not compared (77).
- `-Record` re-records and re-stamps.

`Tools/stage_task.ps1` performs the owner's hand task over the port and
asserts each step. The `content` label covers Backlot's import itself.

`Tools/playground_edit.ps1` does the same for Playground's Scene View: a
pick, the gizmo, then a fresh run held against the baseline at tolerance 0.

## The input

`Tools/input_checks.ps1` (and `.sh`) runs Playground held with
`--port-input` and checks the input layer through `push_input` and
`read_input`, a counted step at a time: the look, the panel's hold on the
mouse, the wheel, the UI's hold on the keyboard and the Scene View's image.
Run it by hand when Platform input or the OS event path changes.
- It needs no hand on the mouse: the run hears no device and its window
  never takes the focus.
- What only a device shows stays a hand check: the OS cursor hiding and
  coming back, raw input, a real change of focus.

## The other machine

Work is built and verified on one machine first; the other backend and the
other platform's scripts are written blind there.
- Whatever only the other machine can verify (a blind backend path, its
  goldens, its scripts, a platform-dependent number) goes into a short list
  file at the repo root.
- An item is added when an increment changes it, and it is measured, never
  predicted.
- The list says what to run and what passing looks like.
- The person on that machine marks each line with the date and the result,
  and a failure becomes a task.

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
