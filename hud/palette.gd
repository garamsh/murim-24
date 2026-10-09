## The colours the HUD draws in: docs/art-direction.md §Palette, each named by its ramp and its
## place along it, dark to light, counted from 1 as art/pixel.lua lists them; then the tints laid
## over a drawing, which are no colour of the palette's.
extends RefCounted

const INK_1 := Color("14101a")
const INK_2 := Color("2a2433")
const INK_3 := Color("3d3647")
const INK_4 := Color("524a5e")
const STONE_3 := Color("66707d")
const STONE_4 := Color("8a94a0")
const GRASS_3 := Color("3b6138")
const GRASS_4 := Color("4f7a3f")
const GRASS_5 := Color("6b9447")
const LEAF_2 := Color("1e3328")
const WATER_3 := Color("2b5770")
const CRIMSON_2 := Color("5c1418")
const CRIMSON_3 := Color("9c1f24")
const CRIMSON_4 := Color("d63a2f")
const AZURE_2 := Color("1f3d7a")
const AZURE_3 := Color("2f63b8")
const AZURE_4 := Color("5b9ae0")
const AMBER_2 := Color("6b4310")
const AMBER_4 := Color("e8a830")
const JADE_2 := Color("2f8c6e")
const JADE_3 := Color("4fc49a")
const JADE_4 := Color("a6f0cf")
const GOLD_2 := Color("8c6a2a")
const GOLD_3 := Color("c99a3c")
const GOLD_4 := Color("f0d080")
const PAPER_1 := Color("a8946c")
const PAPER_2 := Color("d9c9a3")
const PAPER_3 := Color("efe3c2")
const LACQUER_3 := Color("a83a2c")
## Each faction's accent, and the text of its name, a step lighter where the accent is too dark to
## read on ink, by its sprite set in Affiliation.SPRITES.
const ACCENTS: Array[Color] = [AZURE_3, CRIMSON_4, AMBER_4]
const FACTION_TEXT: Array[Color] = [AZURE_4, CRIMSON_4, AMBER_4]
## The tint of a dead person's body, and of an icon not to be had.
const DEAD_TINT := Color(0.45, 0.45, 0.5)
## The tint of a skill's icon not yet learned.
const DIM_TINT := Color(0.5, 0.5, 0.55)
## The shade under the touch controls' wells, and over a skill button's share of its cooldown left.
const SHADE := Color(0.08, 0.06, 0.1, 0.45)
const DIM := Color(0.08, 0.06, 0.1, 0.6)
