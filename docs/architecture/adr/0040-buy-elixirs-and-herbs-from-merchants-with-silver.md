# ADR 0040: Buy elixirs and herbs from merchants with silver

> Status: accepted; its wares as counts superseded by ADR-0050, the dead keeping their silver by ADR-0051, and its sellers by best discipline, fixed prices, purse at birth, wage from nowhere and daily purchase of each ware by ADR-0052
> Date: 2026-10-09

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Issue #167 asks for merchants who sell wares for silver. The owner decided on 2026-10-08 that merchants sell mainly elixirs (단약) and herbs (약초); weapons, equipment and manuals come later. The world had no items and no money. The PM approved the mockup and seven proposals on PR #181 (evidence/pr-181).

Facts that shaped the choices, from `core/` at `1d943f0`:
- Health is restored by a village's apothecary up to `newborn_health`, 120 in the region (ADR 0029). Qi has a store and recovers toward it (ADR 0035).
- Seed 2026's region has no apothecary at year 0, 10, 20 or 30, counted by `structure_kind` over every structure at each point. Villages arm and tend 20 people with no workshop and rarely outgrow that.
- Every person draws a talent limit for each of five disciplines at birth, medicine among them, and the strike rule already names a person's best discipline, its highest limit.
- `offers` lists what a person offers one who talks to it, and the dialogue draws one row for each (ADR 0039).
- Issue #175 may later let a deed take silver, and issue #170 will give people trades.

Options weighed:

- **(a) Merchants by best discipline, with wares, silver, orders and NPC rules in the core.** Chosen.
  - A `Trade` in `Life` holds the wares as data: each ware's name, its price, the health and qi it restores, and the discipline whose people sell it. The region's are a healing herb, 10 silver for 30 health, a quarter of a newborn's, and the Lesser Return Pill, 60 silver for 2,000 qi. Medicine sells both.
  - `sells(person, ware)` is the one rule of who sells what: a living person whose best discipline is the ware's. When #170 gives people trades, the trade replaces the discipline there, and nothing else changes.
  - Silver is a plain count on each person. Each birth holds a purse, 30 in the region, and every living person with a home earns a wage a day, 1 in the region. A buyer's silver goes to the seller, and the dead keep theirs. A deed of #175 can take it without reshaping anything.
  - `buy` and `consume` act at once, as `apply` does. `buy_refusal` and `consume_refusal` name the first reason they are refused, as data.
  - Every person no driver steers follows the same rule once a day: it buys one of each ware it holds none of, from the first person it may buy it from. Each tick it consumes a herb at half its health or below, and a pill at a quarter of its store or below.
  - `offers` adds `Offer::TRADE` for a seller, so the dialogue shows a seller's wares with no branch on the kind of person.
- **(b) Merchants as residents of villages with an apothecary.** Rejected on the measurement above: the region would hold no merchant for at least 30 years.
- **(c) A merchant flag per NPC, or a shop node in the view.** Rejected by the issue. Who sells would be decided outside the world's rules, and NPCs could not buy by the rule the player buys by.
- **(d) Silver as a village's or faction's treasury.** Rejected. #175 needs silver a person holds, and a treasury would need a rule for who spends it.
- **(e) Wares acting on the next tick, as a cast does.** Rejected. Consuming a ware resolves nothing against anyone else, so nothing orders it among the strikes. The NPC rule consumes after damage lands, so a person struck to nothing dies before it can.

The view, approved on PR #181:
- "Show me your wares" is a row of the dialogue's menu.
- The wares screen lists each ware: its icon, name, effect, how many the player holds and its price. The player's silver stands over them, and a line says what was bought or why not.
- The HUD shows the player's silver on a strip under the leader's. On PC a belt right of the technique bar shows the wares held, each under its key.
- Z and X consume the first and second ware, and the HUD notices the result. Touch shows the silver alone while touch is deferred (scope, 2026-10-09).

## Decision

Wares are data in the core's `Trade`, sold by people whose best discipline is the ware's, as the one function `sells` decides. Each person holds silver as a plain count, born with a purse and earning a wage a day at home. Buying and consuming are core orders, refused for their reasons as data, and every person no driver steers buys and consumes by the same rule as the player.

## Consequences

- About a fifth of people sell, those whose best of five disciplines is medicine. A probe of `sells` over every living person in seeds 2026 and 0 to 3, at days of five ticks, counted 22 of 107 at the start and 37 of 222 after 10 years.
- A save holds each person's silver and wares and the trade's data, so `SAVE_VERSION` rises to 8. A world with no trade keeps its hash, the M1 crossing's among them.
- People wounded in a fight heal mid-fight by the herbs they carry. Measured by `make sweep` over seeds 0 to 49 for 100 years against `main` at `1d943f0`:
  - Survival does not change: every faction stays peopled on 48 of 50 seeds in both, and the same two seeds, 29 and 48, fall short.
  - More people live. The living at the end, over every seed, rise from 8,560 to 9,555, up 11.6%, and are higher on 32 of 50 seeds.
  - The realm distribution's shares barely move. From no realm up to peak, they go from 42.5, 22.3, 21.3, 10.9 and 2.9% to 42.0, 21.8, 21.8, 11.3 and 3.1%, and no one passes peak in either.
- A new ware is a row of data and an icon; a new kind of seller is a change to `sells` alone.

## Signs the decision was wrong

- Silver piles up with sellers and nothing spends it, so prices mean nothing by mid-game, and silver needs a sink or a price that moves.
- Herbs consumed mid-fight decide too many fights, duels for a faction's lead among them, so consuming needs a cost in time, or a rule that a duellist does not use one.
- Players look for sellers by what a person does and not by its talent, and the discipline reads as arbitrary before #170 gives people trades.
