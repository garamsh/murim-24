# Evidence for the trade mockup

Mockup of buying elixirs and herbs from merchants with silver, proposed for #167 (phase 1). This branch is never merged.

`trade.gd` made the images. It is the same file as `art/mockup/trade.gd` at a8649d8, the head of `feat/merchants`. I ran it with Godot 4.7.2 on macOS arm64 after `make build`: copied to the project root, then `godot --path . --script res://trade.gd -- <dir>`.

What the images show:
- The world is seed 2026's region with five-tick days, lived 10 years.
- The window and HUD additions use `hud/panel.gd`'s frame, font and palette over the real `play.tscn`.
- The merchant, its name, faction and age are the world's. It is the nearest living person, not hostile to the player, whose highest attainment is medicine.
- The script walks the player's army (10) up to it, 2 cells away.
- The core has none of these yet, so the mockup supplies them, held in the script's `WARES`, `SILVER` and `HELD`: silver, wares, prices, effects, what the player holds, and its wounded health.

Names are `<view>_<run>_<locale>.png`. The runs:
- `pc_1920x1080`: the HUD at 2x in 960x540.
- `touch_1920x1080`: the HUD at 4x in 480x270.

The views:
- `menu` (PC): the dialogue on the merchant, person 12, Orthodox. Its card reads "의술 · 약장수 / Medicine · seller". The menu reads news, wares (chosen), farewell.
- `trade` (PC): the wares. Each has an icon, name, effect, how many are held, and its price. The player's 45 silver is at the top right. The elixir's 60 is greyed: too dear.
- `bought` (PC): after Enter on the herb: 35 silver, 2 held, and "샀다 · 약초 / Bought · Healing herb" in jade. The belt shows Z 2.
- `short` (PC): Enter on the elixir with 35 silver: "은이 모자라다 / Not enough silver" in crimson. Nothing changes.
- `hud` (PC): the player at 58% health. Its silver is on the strip under the leader's, and its wares on the belt right of the technique bar: Z herb 2, X elixir 0, greyed.
- `used` (PC): after Z: 83% health, Z 1, and the notice "약초 · 체력 +30 / Healing herb · Health +30".
- `full` (PC): Z at full health: the notice "체력이 이미 가득하다 / Health is already full". Nothing is used.
- `touch` (touch): the touch HUD with the silver strip and no belt (touch deferred).

The capture's log:

    trade: army 10 of Orthodox talks to person 12 of Orthodox, 2 cells apart
    trade: 0 words overrun their room
    trade: done
