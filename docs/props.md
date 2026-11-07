# Props

The puzzle props built with the `prop` skill (`.claude/skills/prop/`): one
row per prop. `python Tools/gen.py <module>` builds
`Tools/Blender/assets/<module>.py` in Blender 5.2 and writes
`Content/Props/<Family>/<Name>.fbx`, `<Name>.prop.json` beside it and the
preview sheet `captures/prop/previews/<Name>.png`.

| name | family | technique | input | class | rounds | state | notes |
|---|---|---|---|---|---|---|---|
| FloorLever | Interact | kitbash | words | Near | 1 | Pending | The generator's pilot, built from `CHECKLIST.md`'s example lines with no concept sheet. Its four slots bind in palette order in the still and `ModelLoader.GeneratedPropPartSitsOnItsAnchor` pins the Handle on its anchor. Not graded against a sheet. |

`state` ends as `Accept`, `Flawed` (kept, failures named) or `Fallback`;
`Pending` is a prop no owner review has closed yet.

## `<Name>.prop.json`

The card's `moving` list, which the engine's device components read instead
of the FBX: `{"name": <prop>, "moving": [{"name", "kind", "anchor", "axis",
"range"}]}`.
- `name` is the part's node name in the FBX, a child of the root node, with
  its origin on `anchor`.
- The axes are the card's, Blender's: Z up, front +Y, right +X, metres from
  the prop's origin. The loader's model space has a point (x, y, z) at
  (x, z, y), and that swap mirrors, so a hinge's angle changes sign with it.
- `hinge`: `range` in degrees about `axis` through `anchor`,
  counter-clockwise seen from the axis's positive end. `slider`: `range` in
  metres along `axis`. `spin`: turns freely, no `range`.
