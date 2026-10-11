# ADR 0069: Outline the world sel-out and shade it on seven-step ramps hue-shifted from one base

> Status: accepted
> Date: 2026-10-11

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Until now every sprite was outlined in one ink colour, `14101a`, and shaded on hand-picked ramps of three to six steps (`docs/art-direction.md` §Palette). The PM drew one village scene in eight pixel styles and a 3D diorama on a mockup page, `.local/pixel-styles.html`, which was published privately to the owner and is not in git. The owner chose on 2026-10-11 (#252):

- the base is the page's style ②, sel-out outlines with hue-shifted ramps;
- the only HD-2D effects are on lights.

Style ② builds each material's ramp from one base colour, in seven steps:

- shadows turn toward blue-violet (hue 250) and highlights toward yellow (hue 55);
- each outline pixel takes the darkest step of the material it borders, one step lighter on the lit side;
- soft shadows lie under objects.

The 16-pixel grid and Albion's angle (ADR 0059) stay.

The art is drawn by Lua scripts as indices into `art/pixel.lua`'s ramps, and every outline goes through one canvas function. A change of ramps and outline therefore reaches every sprite at once. The scripts then name steps by number, and so do the view's and the HUD's colour constants.

## Considered

- **Sel-out outlines on seven-step ramps built from a base colour, the HD-2D effects only on lights.** The owner's choice. It keeps the grid, the angle and the Aseprite pipeline; the cost is recolouring every sprite once.
- **A 3D diorama, after *Link's Awakening* (2019).** Rejected by the owner:
  - the presentation layer becomes 3D and every piece of art is remade as a model;
  - drawing hundreds to thousands of people on a phone is an unmeasured risk.
- **HD-2D in full, a 3D scene under pixel sprites.** Rejected for the same change of rendering structure. Its bloom, lights and vignette are taken on their own.
- **An NES palette, or high-bit art.** Rejected by the owner.
- **Ink-wash pixel art as the base.** Rejected as the base. It is kept for later as a screen filter for flashbacks, fortuitous encounters and dreams.
- **The mockup's builder as it stands.** Rejected after building the production palette from it:
  - It turns a hue up to about 60° toward its target, the short way round. Azure's highlights went through green to teal (`a8d0f5` to `87d7ba`), and the warm ramps' shadows to crimson (amber's darkest to `5e001b`). That blurs the Orthodox and Unorthodox accents.
  - It lightens a highlight only half the way to white, so the white robe's lightest step fell to grey (`f4f1e6` to `e0e4e2`).
  - Capping the turn at 30° at the ramp's ends, and lightening four fifths of the way, kept each accent in its hue and the robes white.
- **Hand-picked seven-step ramps.** Rejected: nineteen ramps chosen by eye drift apart. A builder with a few numbers restates the rule in one place and re-tunes every ramp at once.

## Decision

- **Ramps.** Every material's ramp is built from its base colour, its middle step, in seven steps dark to light. A step `u` from the middle runs from -6/7 to 6/7, and:
  - its lightness falls toward black by 0.62 `u`, or rises toward white by 0.8 `u`;
  - its hue turns toward 250 in shadow, or toward 55 in light, by at most half the way at 0.43 `u`, and by at most 30° `u`;
  - its saturation is the base's times 1.06, raised by 0.285 `u` in shadow and lowered by 0.2375 `u` in light;
  - a grey base takes saturation 0.114 `u` of the target hue.

  The builder is `art/pixel.lua`'s `build_ramp`, and `docs/art-direction.md` §Palette gives the bases. Fire is an emitter's ramp and is drawn by hand.
- **Outlines.** Bodies, structures, banners, trees and the army marker are outlined sel-out. Each pixel of the one-pixel ring round a silhouette takes the darkest step of the ramp it borders, or the second step where it lies left of or above it, toward the light. The icons, the player's markers, the title and the application icon keep their ink outline: they are HUD and markers, and must read against anything.
- **Shadows.** The contact shadows on the ground, under a body and past a standing structure, are ink by the ordered dither. They are densest at the middle or against the wall and thin outward, so the ground shows through more at their edge.
- **The first pass.** `art/palette_map.lua` maps each former step to the new step nearest it in lightness, keeping steps in order. It rewrites the step numbers the art scripts name and lists what it cannot rewrite for a hand. The view's and the HUD's colours move by the same map.
- **From HD-2D.**
  - Bloom on emitters only (lanterns, lit windows, sword qi), 2D lights at night and indoors, and a light vignette, each switched by the quality profile.
  - Depth of field only in cutscenes and dialogue, never in combat or exploration.
- **Feel.** A hit's hit stop and integer-pixel screen shake are already drawn (#236).

No earlier ADR decided the outline or the ramps; `docs/art-direction.md` held them. ADR 0012's and ADR 0059's grid, angle, view and Aseprite pipeline stand.

## Consequences

- Every colour the view or the HUD names moves to the step its former colour maps to. `hud/palette.gd`'s constants are renamed by their new steps, such as `CRIMSON_5` for the former `CRIMSON_4`. The HUD's colours shift a little, since they come from the same palette.
- A step names a finer shade than before, so a shift of one step shades half as much as a former ramp's step did.
- The ground's ramps stay four steps until the ground's own pull request takes them to seven (#252).
- The mockups under `art/mockup/` draw from frozen copies of the former `pixel.lua` and `sprites.lua`, so they redraw as they were approved. The three that read a production sheet, `genealogy_panel`, `sect_panel` and `arts`, take the sheet as it now is.
- `art/selout_test.gd` checks every world sheet exported for an edge in ink where sel-out applies.
- Bloom, night lights and the vignette land with #209, which already owns the grade, the lights and the fog. Depth of field waits for cutscenes.

## Signs the decision was wrong

- At native size, bodies of one faction in a crowd stop reading apart. The white robes' grey sel-out is the first to watch.
- Most ramps come to need a hand override of the builder, so the rule no longer states the palette.
- A capture of the M1 battle reads muddier than the same capture before, by the owner's or the PM's eye.
