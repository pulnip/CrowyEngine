---
name: prop
description: Build a mechanical prop for the puzzle world from one concept sheet - a gear train, a lever, a switch, a button, a valve, a conveyor, a winch, a gate, or the device a solved puzzle sets in motion. Written as a Blender script that exports an FBX whose moving parts stay separate with named anchors, checked against the concept line by line with numbers, and graded on stills captured in CrowyEngine. Use when someone hands over a concept image or asks for a device, a mechanism, an interactable, a prop, or "make this image a 3D model that moves".
---

# Prop

One concept sheet in, one prop out: built from code, its moving parts separate, graded in the engine. Left alone, the model builds a box with two cylinders that nobody can name. This method makes it spend the budget on what the picture has - the forms in three sizes, the treated edges, the material breaks, the mechanism showing through - and proves it with numbers before anyone looks.

## Where things live

| Place | What |
|---|---|
| `.claude/skills/prop/` | This file, the method in `JOB_DESCRIPTION.md`, `CHECKLIST.md`, `PROMPTS.md`, `LESSONS.md`, `SHOP.md`, and `tools/`. |
| `Tools/gen.py`, `Tools/Blender/` | The generator: a module per prop, built with the kit, exported as FBX with a preview sheet. Ported from `../Backlot/Tools/Blender/` with parts, anchors and shop materials added. If it is missing, porting it is the first job and nothing below runs. |
| `Tools/Blender/assets/<module>.py` | One prop: its `SPEC` card and `build(kit)`. |
| `Content/Props/<Family>/<Name>.fbx` | The export, plus `<Name>.prop.json` with the moving parts. |
| `Content/Props/Shop/<Name>.json` | The shop: one engine material per name in `SHOP.md`. |
| `Art/Codex/Props/<Name>/` | The concept sheet and its prompt sidecar. |
| `docs/props.md` | The assets record: one row per prop (name, family, technique, input, class, rounds, state, notes). |
| `captures/prop/` | Scratch (git-ignored): scene files, stills, stacks, grids. |

## The method

The five docs are the method. Read them before the first prop and keep `JOB_DESCRIPTION.md` open the whole time.

| File | What it holds |
|---|---|
| `JOB_DESCRIPTION.md` | The brief: the look, the order of work, the still, the motion, the hand-over. Every correction the owner gives goes into it, one line per mistake. |
| `CHECKLIST.md` | How to write 10 to 12 numbered lines on what one concept stands on, a template and a full example. |
| `PROMPTS.md` | Every prompt, from the concept sheet to the hand-over. |
| `LESSONS.md` | Every mistake so far and the line it added. |
| `SHOP.md` | The materials, with the engine's numbers. |

The order, every time:

1. **The concept sheet.** Three flat views (front, side, top) and one front 3/4 hero, from Codex with the prompt in `PROMPTS.md`. It sets the look and the proportions; never import it, rebuild it.
2. **Measure the sheet.** `tools/measure.py --grid` on the hero: the bounding box, the edge share, the dark share, and a 10 x 10 grid to read every proportion off. Write the numbers down as shares of the prop's height.
3. **The card and the checklist** for this prop, before the first line of code.
4. **The still loop.** Build, export, capture one still in the engine at the hero angle, stack reference | still | an approved still, plus the stack at 900 px wide, grade every checklist line yes / partial / no against the sheet. **Stop after round 2 and show the owner.** Nothing moves until they say yes.
5. **The motion check.** A sheet of the prop at state 0, 0.25, 0.5, 0.75, 1 for every moving part: the part turns about its anchor and axis, stays inside its range, passes through nothing, and its stop is a shape.
6. **The hand-over.** The row in the assets record, the `.prop.json`, and the lesson if there was one.

## How a prop is built

One module per prop. The card is the contract, the build is the shape, the kit is the only way to make geometry.

```python
SPEC = {
    'name': 'FloorLever', 'family': 'Interact',
    'purpose': 'a floor lever the player pulls between two stops; drives a gate',
    'size': {'width': 0.6, 'depth': 0.8, 'height': 1.2},        # X, Y, Z metres in Blender
    'palette': ['Stone', 'Bronze', 'Iron', 'Glow'],              # shop names; this order is the FBX slot order
    'class': 'Near',
    'parts': ['stepped stone pedestal ...', 'bronze quadrant with 5 notches ...', ...],
    'moving': [
        {'name': 'Handle', 'kind': 'hinge', 'anchor': (0.0, 0.0, 0.45), 'axis': (1, 0, 0), 'range': (-35.0, 35.0)},
    ],
    'tags': {'technique': 'kitbash', 'input': 'concept'},
}

def build(kit):
    kit.add_block('Stone', 0.6, 0.6, 0.3, bevel=0.015)
    with kit.part('Handle', anchor=(0.0, 0.0, 0.45)):            # its own object, origin at the anchor
        kit.add_cylinder('Iron', radius=0.02, height=0.7, ...)
```

- Blender: Z up, front faces +Y, right side +X, metres. The loader converts to the engine's Y-up; `Engine/Content/Test/ModelLoaderTest.cpp` pins where each axis lands.
- `moving[].kind` is `hinge` (degrees about `axis` through `anchor`), `slider` (metres along `axis`) or `spin` (free rotation, no range: gears, drums). The generator writes them to `<Name>.prop.json`; the engine's device components read that file, never the FBX.
- A part that moves is its own object named as in `moving`, parented under the root, with its origin at the anchor. Everything that does not move merges into the root mesh.
- A face is a shop name. The generator refuses a name the card's `palette` does not list.

## Rules that decide whether it works

1. **Start from the sheet, never from imagination.** Asked for "a lever" with nothing to match, the model returns a pole on a box. With a sheet and a checklist it rebuilds what is in front of it.
2. **Every vague word gets a number.** "Chunky" is a width share. "Smooth here" is a chamfer in metres. "Metal" is a shop name. A line without a number cannot be graded and will not be met.
3. **Measure, don't eyeball.** Features pass the checklist while the whole reads as a grey box. Edge share must reach 0.8 of the sheet's; proportions must match within 0.03 of the height.
4. **Forms in three sizes.** The primary mass, secondary forms at 0.2 to 0.5 of it, tertiary details at 0.02 to 0.06 of it. A prop with only the first size is a placeholder however good its material is.
5. **Every hard edge is treated.** A chamfer of 0.01 to 0.03 of the part's smallest dimension on every edge a camera can reach. A razor edge reads as a box at any distance.
6. **Two to four shop materials, never one.** Stone against bronze, bronze against iron, one glow. A material break is a shape read the silhouette cannot give.
7. **A moving part shows how it moves.** Its axle, its bearing collar and its stop are shapes on the model, and it is its own part with an anchor, an axis and a range in the card.
8. **Review after round 2, two props at a time.** A note after round 2 changes the outcome; after round 6 it only costs tokens.

## The still

The engine captures a frame of a scene file; the window opens on display 1 and closes after the frame. Resolution is the window's, 1280 x 720. Run from the repo root.

```
powershell -NoProfile -File .claude/skills/prop/tools/still.ps1 -Mesh Props/Interact/FloorLever.fbx -Materials Stone,Bronze,Iron,Glow -Az 35 -El 20 -Dist 3 -Target 0,0.6,0 -Name FloorLever-r1
```

It writes `captures/prop/<Name>.json` (the scene: camera, sun, floor, the prop), runs `Game.exe` with `--capture-frame 60 --exit-after-frame 60`, converts the BMP and prints `captures/prop/<Name>.png`. The camera orbits the target: `-Az 0` stands on the engine's -Z side looking at +Z, `-El` is degrees above the floor. The hero angle is az 35, el 20 at a distance that fills 60 to 70 % of the frame's height.

- `-Materials` is the card's `palette` in order: the engine binds materials by slot index, in the file's material order, and never reads the FBX's own material values.
- `Mesh` is relative to `Content/`, forward slashes, no drive letter. The FBX must be there.
- Two stills per round: the hero with the floor for the stack, and the same angle with `-NoFloor` for `measure.py`, whose background is read off the corners and must be the flat sky. A floor in the frame turns into foreground.
- `python tools/prop_scene.py` alone writes the scene file for a run by hand; `--state` is not a thing yet, the motion check moves the part through the editor port.

## Review loop

You cannot judge a model from its code; you can look at stills. Never report a prop as done without reading a stack of the current export.

1. `still.ps1` at the hero angle, then `tools/stack.py` with the sheet's hero, the still and the approved still: one row, plus the same row 900 px wide.
2. `still.ps1 -NoFloor` at the same angle, then `tools/measure.py <hero> <floorless still>`: edge share and dark share, yours against the sheet's, and the bounding-box aspect.
3. Grade every checklist line yes / partial / no, one line each, against the sheet and not against your memory of the build.
4. Zoom: `still.ps1 -Dist 1.2` at the signature feature; a second angle at az -45 for the side the hero hides.
5. Fix the no's, then the partials; rebuild; back to 1. Stop at round 2 for the owner, then when a full pass finds nothing.

Checklist for grading a stack:

- **The read.** At 900 px wide, a stranger names the prop and its moving part.
- **Forms.** Three sizes present; the secondary forms sit on the primary mass, not beside it.
- **Edges.** No razor edge where the light hits; chamfers catch a highlight.
- **Materials.** Every shop name in the card shows; the break between two follows a shape.
- **Mechanism.** A window, a gap or a cut shows the mechanism; the axle and the stop are visible.
- **Density.** Edge share at least 0.8 of the sheet's; raised by adding forms and cuts, never by noise.

## Traps the engine and the pipeline already have

- Materials bind by slot index in the file's material order. The card's `palette` order is the slot order, and the still's `-Materials` must repeat it. A wrong order is a stone handle on a bronze pedestal, with no error.
- The FBX's own roughness, metallic and colour never reach the engine. The shop JSON is the material.
- An asset ref is relative to `Content/`, with `/` and no `:`; an absolute path is refused and the mesh is drawn as `engine:Builtin/Missing`. Read the log for that name after every still.
- `--capture-frame` must be at or before `--exit-after-frame`, the capture path must be absolute, and the run is windowed: do not start two at once.
- Captures are deterministic (fixed step), so two stills of the same export are byte-equal: compare rounds with `ImageCompareCheck --edges`.
- The Blender preview sheet is Workbench, flat and orthographic. It is for the modeler's own eyes between runs; a grade comes from the engine still only.
- The kit's bevel is a chamfer computed in code; there are no modifiers. Round forms come from `add_lathe`, `add_cylinder` and `add_tube`, and a visible roundness needs 24 segments or more at Near.
- `[zfight]` from the generator means two faces share a plane. Fix it; the engine shows it as flicker.
- `*.fbx` and `*.png` are Git LFS. A new file lands in LFS by the attributes, nothing to do, but a `git diff` on them is empty.

## Taste, with numbers

- Hero: front 3/4, az 35 el 20, the prop filling 60 to 70 % of the frame height, state 0.
- Chamfers 0.01 to 0.03 of the part's smallest dimension; grooves and inlays 0.02 to 0.06 of the primary mass.
- Gear teeth at least 0.05 m on anything seen from the player's 6 m; tooth count 9 to 21, odd on one of two meshing gears.
- A lever handle 0.9 to 1.2 m tall, grip 0.03 to 0.04 m thick; a button 0.3 to 0.5 m across, travel 0.05 to 0.08 m; a switch arm 0.25 to 0.4 m.
- One glow, and it marks the thing that is powered right now.

## Credits

Forked from [Nyxera0x/cutaway](https://github.com/Nyxera0x/cutaway) (MIT), which builds on [whaleyxbt/claude-motion](https://github.com/whaleyxbt/claude-motion) (MIT): the order of work, the checklist discipline, the measurement rule and the review loop come from there. The Blender kit, the card format and the preview sheet come from `../Backlot` (its `CLAUDE.md` and `docs/plan.md` chapters 5 and 6).
