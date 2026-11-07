# The shop

A face is a shop name; a shop name is one engine material in `Content/Props/Shop/<Name>.json`. The card's `palette` lists the names it uses, in slot order, and the still's `-Materials` repeats that order. Add a material here and as a file, never inline in a card.

Values are the engine's material document (version 2): albedo in linear RGB 0 to 1, metallic, roughness, emissive in linear RGB.

| Name | Where | Albedo | Metallic | Roughness | Emissive |
|---|---|---|---|---|---|
| Stone | housings, pedestals, floors of a device | 0.42 0.40 0.36 | 0 | 0.92 | 0 |
| StoneDark | recesses, slots, the inside of a window | 0.20 0.19 0.17 | 0 | 0.95 | 0 |
| Bronze | quadrants, plates, gears, drums | 0.55 0.36 0.18 | 1 | 0.42 | 0 |
| BronzeWorn | verdigris in recesses and under rims | 0.30 0.42 0.36 | 0.6 | 0.70 | 0 |
| Brass | small bright parts: collars, bands, inlaid frames | 0.80 0.62 0.28 | 1 | 0.30 | 0 |
| Iron | axles, handles, rivets, stops | 0.16 0.16 0.17 | 1 | 0.55 | 0 |
| Wood | grips, belts' slats, crate-like parts | 0.36 0.22 0.11 | 0 | 0.80 | 0 |
| Leather | belts, straps, seals | 0.22 0.12 0.07 | 0 | 0.75 | 0 |
| Glow | the one accent: inlaid lines, the engaged notch, a conduit | 0.10 0.60 0.70 | 0 | 0.40 | 0.6 3.6 4.2 |

## Rules

- Two to four names per prop. One is a placeholder; five is clutter.
- Stone is matte (roughness 0.85 or more); every metal is metallic 1 except BronzeWorn, which is the oxide, not the metal.
- BronzeWorn sits only where water would sit: recesses, under rims, the bottom of a plate. A whole face of it is moss.
- Glow is the only emissive, and it marks what is powered or moving now. One line, one notch, one ring. Never dots, never a face.
- A translucent part (a glass drum, a lens) uses `engine:Builtin/Translucent` until the shop has its own.
- A new material needs a row here, a file in `Content/Props/Shop/`, and one prop that uses it.
