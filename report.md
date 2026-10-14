# #184 phase B mockup: attack motion at the 2.5D angle

Branch `art/attack-motion` at `8231b78d`, drawn by `art/mockup/attack_motion.lua` from the production body template (`art/sprites.lua`) and palette (`art/pixel.lua`, ADR 0069 ramps), sel-out in each garment's darkest step.

- `attack_motion_4x.png`: the contact sheet at 4x. It shows every body frame on grass at native size, the effects, the damage-number colours, and a swing landing on a foe as a four-step sequence.
- `attack_frames_6x.png`: every body frame at 6x, Orthodox, Demonic and Unorthodox rows, in the order idle, swing, lunge, cast, hurt, throw, mend, torch.
- `attack_<faction>.png` / `.json`, `attack_effects.png` / `.json`: the tagged sheets as Aseprite exports them for the build.

Decisions for approval:
1. Body frames stay 16x24, as art/bodies' sheets, so BodyView's atlas and bands are unchanged. A swing's reach beyond the frame is the slash effect's, drawn by the view from the core's strike as phase A's arc is.
2. Motion per use. A melee arc plays swing; a TARGETED use (guard) plays lunge; an AREA use plays cast; a LINE use (the darts) plays throw; mend and torch play their own. The struck play hurt.
3. Effects in the user's faction accent: azure for the Orthodox (sword qi, as art-direction.md has it), crimson for the Demonic, amber for the Unorthodox.
   - the slash is a three-frame crescent with a white-hot edge first;
   - an area use is a ring of force at the angle with a white-hot core and sparks;
   - the darts are eight headings and a streak;
   - a hit spark plays on the struck;
   - mend is jade motes rising off a struck structure; torch is flames with smoke on it.
4. Damage numbers by technique: Plum Blossom azure, Heavenly Demon violet (crimson stays for hostility and the player's loss), Black Fang amber, the guard strike paper. The player's own loss stays crimson, as phase A has it.
5. Timing: 80 ms a frame for the motions and 60 ms for the effects, so a swing completes within the 200 ms the strike frame shows today.
