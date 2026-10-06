# ADR 0019: Draw talent and lifespan at birth, and bear a soul again into the same world

> Status: accepted; its day fixed at 1,800 ticks in the core superseded by ADR-0021
> Date: 2026-10-07

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The concept settles talent, lifespan and reincarnation (`docs/concept.md` §Talent, §Lifespan, §Reincarnation):
- Talent is a hidden growth and limit per discipline, drawn at birth.
- Lifespan is drawn at birth and grows with attainment.
- On death the player is born again into the same world, which never resets.
- A past life widens the next talent draw, mainly in the disciplines it practised, a little at the top, and less with each life.

Issue #42 asks for these as one birth rule for the player and every NPC, on the persons of ADR 0017. Its PM scope adds two things:
- A calendar the HUD reads.
- A minimal family: parents and a family recorded at birth, and the player born again into an existing family.

Options weighed:

- **The distributions.** Each draw is uniform over a mean plus or minus a spread, as integers from the seed. The moments of a uniform draw are exact, so a test checks them with no tolerance for rounding. Rejected: a normal approximation, which needs floats or a sum of draws for no rule that reads the shape yet.
- **Where life is data.** One `Life` given with the world, as the region's table gives it:
  - the draw for each discipline
  - the lifespan in days
  - the days each point of attainment adds
  - the two widenings
  - a newborn's health

  A world given no `Life` draws nothing and ages no one. Rejected: talent fields on `ArmySpec` or `Recruit`, which every brace-initialised army in the code would have to name.
- **Which discipline a strike practises.** Skills have no discipline yet, so a strike that hits anyone or anything practises the striker's best discipline, its highest limit, the lowest index on a tie. This holds until martial arts give each technique its discipline (`docs/concept.md` §Martial arts, still a draft). Rejected: a discipline field on `Skill`. That would change every skill site in the code for a mapping no technique defines yet.
- **Ageing.** A person dies when its age in days passes its lifespan plus `days_per_attainment` days for each point of attainment. It dies as a wound kills, at no health, so the dead keep one path. Rejected: a separate dead-of-age state, which every reader of health would have to know.
- **The soul.** A record of the lives lived and the disciplines practised, which a person may carry. `ensoul` gives one to a living person, and the adapter gives one to the player's body.
  - The birth rule reads only the soul's history. An NPC carries no soul and is born, like everyone the world starts with, from an empty history, through the same function.
  - Rejected: a player flag on the person, which is a player branch in the birth rule.
- **The widening.** A discipline the soul practised draws its limit `discipline_widening / lives` further down and up. Every discipline draws `upper_widening / lives` further up. The integer division shrinks the widening as lives repeat, and nothing raises the bottom. Rejected: widening growth as well. A second widening has nothing yet to tell it apart from the first.
- **Where the soul is born again.** The soul is born on the tick after its body dies:
  - in a village drawn by the seed among those with a living resident
  - into the family of its first resident, with its first two residents as parents
  - into the army its body served in, which takes the residents' faction and skills and is bound for the birthplace

  The world is the same object, so its hash goes on from where it was. Rejected:
  - A new army per birth, which shifts no index but makes every driver find its body again.
  - Drawing a faction and then a village, which weights a faction of few villages above one of many. A village is already a faction's, so drawing the village gives a random faction.
- **The family.** A family is an id that a newborn takes from its parent. Everyone the world starts with belongs to the family of the army it was created in, and has no parents. Rejected: a family record with its own table, which no rule reads yet.
- **The calendar.** `CALENDAR` in the core holds the data: a day is 1,800 ticks, a minute of play at 30 ticks a second; a season is 30 days, and a year is 4 seasons. `date()` derives the year, the season and the day from the tick count, so the calendar adds no state. Rejected: a calendar in GDScript, which would be a second clock beside the core's.

## Decision

Every birth, of the player's body or of an NPC, draws talent per discipline and a lifespan from the world's `Life` data through one function, given the history of the soul being born, empty when there is none. A soul's next body is born into a family of a village in the same world, and its past lives widen its draw by discipline and a little at the top, less with each life. The core keeps the calendar, derived from its ticks.

## Consequences

- The hash folds talent, attainment, birth, family, parents, lifespan and souls, and the pinned value changes.
- At 60 ± 15 years of 120 days, region people outlive any session played so far. Ageing shows in tests, through `Life` with a lifespan of a day.
- An army's faction and skills can change while the world runs, and the adapter reads them live. Anyone still serving in a soul's army changes with it; the player's body is an army of one, so in play no one else does.
- A soul whose body dies when no village has a living resident is not born again.
- A leader who dies of age leaves its faction led by a dead person, as ADR 0018 leaves a leader dying outside a duel.

## Signs the decision was wrong

- Techniques get disciplines and the best-limit rule has to stay beside them, or players read their practice as going to the wrong discipline.
- Uniform draws read wrong in play: geniuses as common as dullards, where the concept wants rare ranges.
- A rule needs the family as more than an id, such as inheritance, genealogy or a clan's resources, and the id has to grow into a record.
- A soul's army still holds others when its body dies, and their turning to the newborn's faction reads wrong, so the newborn needs an army of its own.
