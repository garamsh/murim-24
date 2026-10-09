# ADR 0021: Let people marry, have children and bear family names

> Status: accepted; its one family per army the world starts with superseded by ADR-0025 for the families of a village's households; its sex drawn at even odds for every birth superseded by ADR-0027 for children where kinship balances sexes; its names held in Korean and romanised forms in the core superseded by ADR-0049, which makes them text keys
> Date: 2026-10-07

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The concept's growing civilization (`docs/concept.md` §Growing civilization) has NPCs meet and have children, with genealogy and families, in a world that never resets. Issue #73 asks for this as data rules, one rule for every person, the player's body included. It also asks for:
- Korean names with romanised forms.
- Births capped by the 500 living bound (MAX_PEOPLE), the only cap until the economy.
- A headless run of a generated region over 30 years showing three generations.

ADR 0019 already gives one birth function that draws talent and lifespan, and records parents and a family. Its calendar fixed a day at 1,800 ticks as a core constant.

Two facts shaped the choices:
- Thirty years is 6.48 million ticks. The seed-24 region (86 people) cost 0.034 ms a tick optimised and 0.206 ms in the Debug test build: over 1,333 s, before any birth.
- Everyone a world starts with was born at tick 0. No one could marry for the 16 years the marrying age needs.

Options weighed:

- **The day's length.** Ticks per day becomes world data in `Rules`: 1,800 by default and in the region's data, so play is unchanged. A test lives the same generated region through 30 years with days of one tick, 3,600 ticks in about 2 s, with every rule counted in days unchanged.
  - Rejected: keeping the constant and moving the 30-year run out of `make ci`, which leaves the acceptance run unchecked by CI.
  - Rejected: a test-only `Life` of day-scale ages, which tests other data than the game's.
- **Founders' ages.** The people a world starts with are born a founding age, drawn from `Life`, before the world begins, so a person's birth tick is signed. Rejected: starting everyone newborn, which leaves a world without marriages for its first 16 years.
- **Sex.** Drawn at birth, even odds, by the one birth function. Marriage is between a man and a woman.
- **Marriage and children.** `Kinship`, optional in `Life`, checked once a day. All ages are in days and chances are per mille a year, rolled daily from the seed.
  - A living person of marrying age, with a home and no living spouse, marries with the marriage chance.
  - The partner is drawn by the seed among those free to marry, of the other sex and another family, whose home is the same village or one whose centre lies within the neighbourhood.
  - A wife of bearing age, whose husband lives, bears a child with the birth chance, no sooner than the spacing after her last, while fewer than MAX_PEOPLE live.
  - The child is born through the one birth function. It takes its father's family, or its mother's if the data says so, and that parent's side, home and army.
  - Every marriage is recorded on both spouses, in order, so a widow's earlier husbands stay known.
  - Rejected: a marriage only within a village, which a village of one family can never meet.
  - Rejected: a separate birth path for children, which would break ADR 0019's one rule.
- **Names.** `Names` in `Life` holds surnames and given names for each sex, each in Korean with a romanised form.
  - Each family, one per army the world starts with, draws a surname by the seed as the world begins.
  - Each birth draws a given name for its sex. A child of the father's family bears his surname.
  - Rejected: a name stored on each person as text, which duplicates the family's surname into every member.
- **Read ports.** The core gives each person's name, and its parents, spouses, children and family on the person. The adapter reads `person_name`, `person_parents`, `person_children`, `person_spouse` and `person_family`. No UI reads them yet.

## Decision

People marry and have children by data-defined kinship, the same rule for every person, and every child is born through ADR 0019's one birth function while fewer than 500 people live. Every person bears its family's surname, drawn when the world begins, and a given name drawn at birth, in Korean and romanised. A day's ticks become world data, and the people a world starts with are born a founding age before it. This supersedes ADR 0019's fixing of the day at 1,800 ticks in the core.

## Consequences

- Over 30 years with days of one tick, generated regions on seeds 24, 2026, 7, 99 and 5 reach a third generation in all of their six to eight families but at most one. The living peak at 194 to 365, within 500. With every wife bearing each day she may, a region meets 500 and holds there.
- Each day a world with kinship scans the living once, and the partners for whoever marries. At 500 people that is cheap next to the movement on every tick.
- The pinned determinism hash changed: every birth now draws a sex and a given name, and the hash folds them with spouses, children, surnames and the day's length.
- The player's body marries by the same rule once it has a home. As `born_in` makes it, it has none until it is born again into a village, so it does not marry yet.

## Signs the decision was wrong

- Families read as clans, everyone in a village sharing one surname, and players expect households within them, so a family has to become more than an id per army.
- The population settles at the 500 bound in play long before the economy arrives, and births need a softer cap.
- Founders' founding ages read wrong, such as a village of only adults, and they need ages from a wider distribution or a seeded past.
- The player expects to marry from the start, and the home requirement keeps its body single.
