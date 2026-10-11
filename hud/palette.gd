## The colours the HUD draws in: docs/art-direction.md §Palette, each named by its ramp and its
## place along it, dark to light, counted from 1 as art/pixel.lua lists them; then the tints laid
## over a drawing, which are no colour of the palette's.
extends RefCounted

const INK_1 := Color("171229")
const INK_2 := Color("221b35")
const INK_4 := Color("3d3548")
const INK_5 := Color("716273")
const STONE_3 := Color("69708d")
const STONE_4 := Color("8994a1")
const GRASS_3 := Color("39603d")
const GRASS_4 := Color("4e7c3d")
const GRASS_5 := Color("7eab58")
const LEAF_2 := Color("181f2e")
const WATER_3 := Color("2f5579")
const CRIMSON_2 := Color("631648")
const CRIMSON_4 := Color("a01b21")
const CRIMSON_5 := Color("d6422e")
const AZURE_2 := Color("1e2b77")
const AZURE_4 := Color("2b62bc")
const AZURE_5 := Color("5c99ca")
const AMBER_2 := Color("5d2625")
const AMBER_5 := Color("e2a32c")
const JADE_2 := Color("2b7287")
const JADE_4 := Color("4bc89b")
const JADE_7 := Color("deebc9")
const GOLD_3 := Color("a36734")
const GOLD_4 := Color("cd9b38")
const GOLD_6 := Color("e3ce93")
const PAPER_3 := Color("bc9a7d")
const PAPER_4 := Color("dbcaa1")
const PAPER_6 := Color("efe5c9")
const LACQUER_5 := Color("b2563d")
## Each faction's accent, and the text of its name, a step lighter where the accent is too dark to
## read on ink, by its sprite set in Affiliation.SPRITES.
const ACCENTS: Array[Color] = [AZURE_4, CRIMSON_5, AMBER_5]
const FACTION_TEXT: Array[Color] = [AZURE_5, CRIMSON_5, AMBER_5]
## The tint of a dead person's body, and of an icon not to be had.
const DEAD_TINT := Color(0.45, 0.45, 0.5)
## The tint of a skill's icon not yet learned.
const DIM_TINT := Color(0.5, 0.5, 0.55)
## The shade under the touch controls' wells, and over a skill button's share of its cooldown left.
const SHADE := Color(0.08, 0.06, 0.1, 0.45)
const DIM := Color(0.08, 0.06, 0.1, 0.6)
