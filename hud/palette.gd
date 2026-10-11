## The colours the HUD draws in: docs/art-direction.md §Palette, each named by its ramp and its
## place along it, dark to light, counted from 1 as art/pixel.lua lists them; then the tints laid
## over a drawing, which are no colour of the palette's.
extends RefCounted

const INK_1 := Color("1b1823")
const INK_2 := Color("26222f")
const INK_4 := Color("3d3548")
const INK_5 := Color("6c5c79")
const STONE_3 := Color("6c7789")
const STONE_4 := Color("8994a1")
const GRASS_3 := Color("376830")
const GRASS_4 := Color("4e7c3d")
const GRASS_5 := Color("77a95a")
const LEAF_2 := Color("182f29")
const WATER_3 := Color("2d5e7b")
const CRIMSON_2 := Color("6e0b2b")
const CRIMSON_4 := Color("a01b21")
const CRIMSON_5 := Color("d53d30")
const AZURE_2 := Color("142f81")
const AZURE_4 := Color("2b62bc")
const AZURE_5 := Color("5496d2")
const AMBER_2 := Color("7d2b04")
const AMBER_5 := Color("e1a12e")
const JADE_2 := Color("228f87")
const JADE_4 := Color("4bc89b")
const JADE_7 := Color("cbead1")
const GOLD_3 := Color("b16f26")
const GOLD_4 := Color("cd9b38")
const GOLD_6 := Color("decb98")
const PAPER_3 := Color("cba26d")
const PAPER_4 := Color("dbcaa1")
const PAPER_6 := Color("e9e3ce")
const LACQUER_5 := Color("b74d37")
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
