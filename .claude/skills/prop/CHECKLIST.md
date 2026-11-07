# The checklist

The job description covers every prop. The checklist covers one sheet. Write it before the first line of code: what does this sheet stand on, in numbered lines a reader could check against it?

## Rules for writing one

- **10 to 12 lines.** Fewer misses features; more turns into a spec nobody grades.
- **Order:** silhouette and proportions first, then the signature feature, then the moving parts, then forms and edges, then materials, then the mechanism, then the accent, then density.
- **Every vague word gets a number.** "Chunky pedestal" became "pedestal width 0.5 of the height" after a pedestal came back as a post.
- **Write "never" next to the default you are fighting.** "A window over the gears, never a sealed housing." Without it the housing seals itself.
- **Proportions in numbers, read off the grid.** Height is 1.0; every other measure is a share of it.
- **The moving part gets its own line** with its anchor, axis and range, because that line is what the engine will use.
- **The last line is always density:** the edge share against the sheet's.

## Template

```
1. Silhouette and view: [the shape, what it stands on, the hero angle]. Proportions off the grid: [width / height, the signature feature / height, the moving part / height].
2. The signature feature: [the one thing a viewer remembers: a quadrant of notches, a glass drum, a ring of teeth]. [Size in numbers.]
3. The moving part: [name], a [hinge / slider / spin] about [anchor] along [axis], range [numbers]; its stop is [a shape]. NEVER [the default that kills it].
4. Forms in 3 sizes: [the primary mass], [secondary forms at a share], [tertiary details at a share]. Never one block.
5. Edges: chamfer [metres] on every edge; [the rounded forms and their segment count].
6. Materials: [2 to 4 shop names and where each one is]. Never one material.
7. Mechanism: [the window, gap or cut and what shows through it, in counts].
8. [The second feature.] NEVER [its default].
9. Small parts: [rivets, collars, bands, inlays, with sizes and counts].
10. Glow only as [lines or inlays, exactly where]. No dots.
11. At 900 px wide it reads as [the prop] with [the moving part] visible.
12. Edge share at least 0.8 of the sheet's ([the sheet's number]).
```

## Example: a floor lever on a stone pedestal, 12 lines

```
1. A floor lever: a square stepped pedestal 0.5 of the height wide, the bronze quadrant rising from its back edge to 0.7, the iron handle to 1.0. Front 3/4 from az 35, el 20, standing on the floor plane.
2. The signature feature: a bronze quadrant plate with 5 notches along its arc, 0.35 of the height in radius, 0.04 thick, the notches 0.03 deep.
3. The moving part: Handle, a hinge about the quadrant's axle (0, 0, 0.45) along X, range -35 to 35 degrees; its stops are the first and last notch. NEVER a handle planted in the pedestal with no axle.
4. Forms in 3 sizes: the pedestal (3 steps, the top one 0.8 of the bottom), the quadrant and its two bearing blocks at 0.3, the rivets, the grip ring and the notch bevels at 0.03. Never one block with a pole.
5. Edges: 0.015 chamfer on every pedestal edge, 0.008 on the quadrant; the axle, grip and collars as 24-segment cylinders.
6. Materials: Stone on the pedestal, Bronze on the quadrant and bearing blocks, Iron on the axle, handle and rivets, Glow in the notch that is engaged. Never one material.
7. Mechanism: the axle passes through both bearing blocks and shows 0.03 beyond each; a slot in the pedestal top under the quadrant shows the pawl.
8. The pedestal top is a separate slab 0.05 thick overhanging the steps by 0.02 each side. NEVER a flush extrusion.
9. Small parts: 4 rivets 0.02 across on each bearing block, a collar 0.05 wide at each end of the axle, a grip ring 0.04 tall under the knob.
10. Glow only as the engaged notch and one inlaid line down the quadrant's rim. No dots.
11. At 900 px wide it reads as a lever with a notched quadrant, the handle clearly at one end of its range.
12. Edge share at least 0.8 of the sheet's (0.142).
```

Line 2 and line 4 are where the first round usually fails: the quadrant comes back as a flat fan with no notches, and the pedestal as one box. With the numbers, round 2 has the notches and the steps.
