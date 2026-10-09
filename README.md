# Evidence for the nameplates mockup

Mockup of names over the people near the player, with hostile people marked in red, proposed for #164 (phase 1 of PR #199). This branch is never merged.

`nameplates.gd` made the images. It is the same file as `art/mockup/nameplates.gd` at 9bde82c, the head of `feat/nameplates`. I ran it with Godot 4.7.2 on macOS arm64 after `make build`: copied to the project root, then `godot --path . --script res://nameplates.gd -- <dir>`.

What the images show:
- Seed 2026's region in the real `play.tscn` at 1920x1080, the HUD at 2x in 960x540. The player is Orthodox.
- The crowd is staged. Every two factions hostile by stance make peace, so the Demonic Cult stands neutral toward the Orthodox. Armies are walked round the player: Orthodox below it, Demonic to its left, Unorthodox to its right. Once they arrive, the Orthodox leader declares war on the Unorthodox, who then stand at war. The world then stops, so every body is in its standing frame.
- 42 people stand within 8 cells of the player in every image. Named, by relation: ko 12 own, 9 neutral, 5 at war, 16 left out; en 7 own, 6 neutral, 4 at war, 25 left out. English names are wider, so more are left out.
- Hostility is `WorldAdapter.relation(player's faction, the person's army's faction)`, `RELATION_HOSTILE` or `RELATION_AT_WAR` (ADR 0038). The core already has every read needed: `relation`, `army_faction`, `roster` and `person_name`. No adapter read is added.

Names are `<variant>_<locale>.png`. The variants differ only in the hostile body's outline:
- `outline`: the body's own one-pixel ink outline is drawn in crimson instead of ink.
- `ring`: a one-pixel crimson ring outside the ink outline.

Proposals for the PM:
1. **Reach: 8 cells**, a straight-line distance from the steered person, held as a constant in the view (`NAME_REACH`). Nobody past it is named. The player itself is not named: the HUD names it and its ring and chevron mark it. The person the talk prompt names is not named again.
2. **Colours** (`hud/palette.gd`, all from `docs/art-direction.md` §Palette): own and ally in jade `4fc49a` (JADE_3); neutral in paper `efe3c2` (PAPER_3), the HUD's text colour; hostile and at war in crimson `d63a2f` (CRIMSON_4). Each name has a one-pixel ink outline (INK_1), so it reads over grass, roofs and bodies. Jade is the art direction's colour for the player's markers and selection; this extends it to people on the player's side. The art direction gains a line for it.
3. **Outline: `ring`** (recommended). The art direction gives every body a one-pixel ink outline, and `ring` keeps it, adding the crimson outside. That reads more strongly than `outline`, which only recolours it. The body frames have a free pixel above and beside every silhouette but none under the feet, so the ring has no bottom edge under the feet.
4. **Names never overlap.** Nearest first. A name is left out if it would cover a nearer person's name, a HUD panel, or the talk prompt's slip. That is why crowds above show gaps; a hostile person left unnamed still has its red ring.
5. **Names are drawn in the HUD's pixels**, the HUD's font at the HUD's size at every window, under every HUD panel and screen. They are not drawn while a screen is open over the world.
6. **The frame bench measures with names on.** `world_view.tscn`, which `make bench` renders, names the people round the M1 battle's centre, read against one of its factions.
