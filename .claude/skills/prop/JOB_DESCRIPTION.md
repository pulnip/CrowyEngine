# Job description

Read this before every prop. When the owner corrects you, the correction goes into this file, one line per mistake. The look below is the default for the puzzle world's ancient machinery; a family's first approved sheet overrides it for that family, and the override is written here under the family's name.

```
You build mechanical props for a physics-puzzle world from a concept sheet, as a Blender script the kit exports to FBX, and you grade them on stills from CrowyEngine.

THE LOOK
- ancient machinery: massive carved stone housings, a bronze mechanism, dark iron on every axle and every stop
- forms in 3 sizes: the primary mass, secondary forms at 0.2 to 0.5 of it, tertiary details at 0.02 to 0.06 of it
- every hard edge chamfered at 0.01 to 0.03 of the part's smallest dimension; a razor edge is a bug
- the mechanism shows: windows, gaps and cuts in the housing over gears, axles and drums; never a sealed box with a handle on it
- a moving part shows its axle, its bearing collar and its stop as shapes
- 2 to 4 shop materials per prop: stone against bronze, bronze against iron, one glow; never one material
- verdigris (BronzeWorn) only in recesses and under rims, never over a whole face
- glow (Glow) only as lines and inlays that mark what is powered or moving now; never dots, never a whole face
- repetition is for teeth, notches and rivets; plates, blocks and windows come in different sizes
- scale in metres, built for a 1.8 m player: a lever grip at hand height, a button a boot can press, a gear tooth a 6 m camera can count

THE ORDER
1. Measure the sheet first: tools/measure.py --grid on the hero view. Height, width, the signature feature, the moving part, each as a share of the prop's height. Write the numbers down.
2. Write the card: name, family, purpose, size, palette in slot order, class, parts, moving (name, kind, anchor, axis, range).
3. Write the checklist for this sheet: 10 to 12 lines, every one with a number.
4. Build every part that moves as its own part with its anchor, so the engine can turn it later.
5. Export, capture one still at the hero angle (az 35, el 20, state 0), and stack the sheet's hero, your still and an approved still, plus the stack at 900 px wide.
6. Grade every checklist line yes, partial or no, against the sheet, not against what you remember building.
7. Measure: edge share and dark share inside the bounding box, yours against the sheet's. Edge share below 0.8 of the sheet's is a no. Proportions the same way, within 0.03 of the height.
8. Stop after round 2 and show the owner. Nothing moves until they say yes.

THE STILL
- the hero is front 3/4 at az 35, el 20, the prop filling 60 to 70 % of the frame height, at state 0
- the second still is az -45 at the same elevation, for the side the hero hides
- the zoom is the signature feature at a distance that fills the frame with it
- the floor is the engine's plane; the sun is the sample scene's; nothing else in the frame

THE MOTION
- one sheet per moving part: state 0, 0.25, 0.5, 0.75, 1, in one row
- a hinge turns about its anchor and axis and stays inside its range; a slider moves along its axis; a spin turns freely
- nothing passes through anything at any state; the stop is a shape the part meets at the end of its range
- two gears that mesh keep their teeth clear at every state; their ratio is the tooth counts in the card

THE HAND-OVER
- a row in the assets record: name, family, technique, input, class, rounds, state (Accept, Flawed, Fallback), notes
- <Name>.prop.json beside the FBX, written by the generator from the card
- the lesson, if the round taught one: what happened, what it looked like, the line it added
```

## Why these lines

- **"Grade against the sheet"** keeps the self-check honest. The model grades its memory of the build as "yes" otherwise.
- **"Nothing moves until they say yes"** keeps tokens from burning on the motion of a box.
- **"The correction goes into this file"** is how prop 5 stops repeating the mistakes of prop 1.
- **"A razor edge is a bug"** because a chamfer is what the eye uses to tell a modelled object from a placeholder, before any material.
- **"The mechanism shows"** because a sealed housing hides the one thing a puzzle prop has to say: how it works.
- **"Measure the sheet first"** because the cutaway piranha passed every feature line and came back a crocodile; the jaw share was never written down.
