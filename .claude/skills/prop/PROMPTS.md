# Prompts

Every prompt in the order you use them. Replace the parts in [brackets].

## 1. The concept sheet

Through the Codex MCP tool `edit_with_codex` (the bridge is `Tools/CodexMcp.ps1`, registered in `.mcp.json`). Write the prompt to `Art/Codex/Props/[Name]/sheet.prompt.md` first, in a ```text block, then send it; add a Result section after. Give absolute output paths. Codex writes the image with its image tool and copies it to the path; if the copy is blocked, copy it by hand from `~/.codex/generated_images/`.

```
Use your image generation tool. Save the result to [absolute path]/sheet.png and do not modify any other file.

A concept sheet of [a floor lever on a stone pedestal] for an ancient-machinery puzzle world: massive carved stone housing, bronze mechanism, dark iron axles, one glowing inlaid line that marks the powered part.
Four views on one wide landscape sheet, the same scale, plain white background, no shadows, no text, no labels, no logo:
left to right, a flat front view, a flat side view, a flat top view, and a front three-quarter view slightly from above.
The mechanism shows through a window or a gap in the housing. The moving part ([the handle]) is drawn at the middle of its travel. Every edge chamfered; forms in three sizes: the housing, the gears and arms, the rivets and inlays.
Industrial design rendering, clean lines, no sketch marks.
```

Pick a prop with a mechanism to show and one moving part to name. A prop whose whole surface is one smooth form (a plain pillar, a bare ball) reads as a placeholder however many rounds it gets.

## 2. The card and the checklist

```
Read JOB_DESCRIPTION.md, CHECKLIST.md and SHOP.md. Run tools/measure.py --grid on the sheet's hero view and read every proportion off the grid as a share of the prop's height.
Write the SPEC card for [the lever]: name, family, purpose, size in metres, palette in slot order from the shop, class, parts, moving (name, kind, anchor, axis, range).
Then write the 10 to 12 line checklist for this sheet. Every line has a number. The moving part has its own line. The last line is the edge share.
Show me both and stop.
```

## 3. The still loop

```
Build [the lever] in Tools/Blender/assets/[module].py and loop:
1. Run the generator, read its [spec], [export] and [zfight] lines.
2. Capture a still at the hero angle: still.ps1 -Az 35 -El 20, the prop filling 60 to 70 % of the frame height.
3. Stack the sheet's hero, your still and the approved still with tools/stack.py, plus the 900 px row.
4. Grade every checklist line yes, partial or no, one line each, against the sheet.
5. Fix the no's, then the partials.
Stop after round 2 and show me the stack and the grades. Wait for my notes before round 3.
```

Then grade it yourself, against the sheet and not against your memory of it.

## 4. Measuring

```
Run tools/measure.py on the sheet's hero view and on your still captured with -NoFloor.
Density: the edge share inside the bounding box. Yours has to reach 0.8 of the sheet's. Raise it by adding forms, cuts, windows, collars and chamfers that catch the light; noise does not count.
Proportions: read width, the signature feature and the moving part as shares of the height, the same way on both. Report both numbers side by side, and the difference.
```

## 5. Round notes

When you send the modeler back for another round, give numbers, not adjectives:

```
Round [3]. Fix these in order, measure each on your still and on the sheet, report both:
1. [The biggest problem, with the target number: "quadrant radius 0.35 of the height, yours is 0.22".]
2. [The next one: "glow as one inlaid line, zero dots".]
3. ...
Grade each item yes / partial / no against the sheet. Save round[3]_still.png and round[3]_stack.png and stop.
```

## 6. The motion check

```
Take the approved export. For every entry in moving, capture the prop at state 0, 0.25, 0.5, 0.75 and 1 and stack the five in one row.
A hinge turns about its anchor and axis and stays inside its range; a slider moves along its axis; a spin turns freely.
Check: nothing passes through anything at any state; the stop is a shape the part meets at the end of its range; two meshing gears keep their teeth clear.
Show me the row and stop.
```

Until the engine's device components read `.prop.json`, the states come from the editor port: open the scene with `Editor.exe <scene> --hold --port-input`, select the part by name, write its transform rotation or position, and `Save-PortFrame`. `Tools/port.ps1` has the verbs.

## 7. The hand-over

```
Write the row in the assets record: name, family, technique, input, class, rounds, state, notes.
Confirm <Name>.prop.json sits beside the FBX and lists every moving part with its kind, anchor, axis and range.
If a round taught a lesson, add it to LESSONS.md and the line it adds to JOB_DESCRIPTION.md.
```
