# Evidence for PR #368

Captures of the dialogue card as `play.tscn` draws it, after #366 gave people occupations.

- `card_capture.gd` took them, run from the repository root at `8e2ac640` on `main` `b2556de`, after `make build`: `godot --path . --script res://card_capture.gd -- <dir>`.
- The world is seed 2026's generated region at its first tick, with the player walked up to each person through the core.
- Every image is PC at 1920 x 1080, and each comes in Korean (`_ko`) and English (`_en`).

| Image | What it shows | Card under the portrait |
|---|---|---|
| `seller_pc_1920x1080_ko.png` | The dialogue open on the nearest person who sells the herb: person 0, a merchant | 정파 · 상인 · 26세, the occupation in jade |
| `seller_pc_1920x1080_en.png` | The same, in English | Orthodox · Merchant · Age 26 |
| `plain_pc_1920x1080_ko.png` | Control: the nearest person who sells nothing, person 1, an innkeeper | 정파 · 문파 없음 · 27세 |
| `plain_pc_1920x1080_en.png` | The same, in English | Orthodox · No sect · Age 27 |

- The seller's menu offers its wares (물건 좀 봅시다. / Show me your wares.).
- The control's card shows its sect line, here no sect, since it sells nothing.
