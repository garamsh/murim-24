# ADR 0068: Surround a foe from attack slots, and let only its token holders strike it

> Status: proposed
> Date: 2026-10-11

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Issue #247. The owner decided on 2026-10-11 to add attack slots and attack tokens to combat:
- **Attack slots.** Around each foe stand a few positions at arm's length. A person closing to strike goes to a free slot, not straight at the foe, so a crowd spreads around a target instead of piling onto one point.
- **Attack tokens.** Each foe grants a small number of tokens, and only a token holder strikes it. The rest hold on a ring just outside, circling and waiting, and take a token when one frees, as in Batman: Arkham and God of War. One against many then reads as a wuxia brawl rather than a pile-on.
- The same rule holds for NPCs and for attacks on the player. The core decides all of it; the view only draws.

Facts from `core/` at `c86b08d` and the open pull requests it builds on:
- Every living person acts on every tick (ADR 0017). People keep the separation, `SEPARATION`, a cell (256 sub-cell units), through a push from each neighbour closer than that, read from the spatial hash rebuilt each tick.
- A person moves `SPEED`, 16 units a tick, or `DIAGONAL_SPEED`, 11 along each axis, on an 8-connected flow field toward its army's objective (ADR 0037). A mover feels no pull while a foe on the move lies ahead within its reach.
- A person no driver steers engages the nearest foe within the rules' sight, 6 cells in the region. It stands to fight while the foe is within its reach, and otherwise takes the open step nearest the line to the foe (ADR 0046's engage rule).
- The basic strike, `MELEE`, reaches a cell and a half (384 units). In ADR 0066 (pull request #237) it is `TARGETED` and aimed at the army's attack-intent target, else at the nearest foe in range, every 15 ticks.
- An attack intent is an order's target, which the army's members chase and strike, steered or not. It ends when the target dies, stops being a foe or leaves sight (the army then holds where its lead stands), or when the player clears it or gives a move order.
- Nothing limits how many strike one person: in the M1 battle, 18 strikes land a tick among 500 people, and a foe at the front of a line takes every strike in reach.
- ADR 0058, proposed, adds levels, cliffs and leaps. A leaping person neither strikes nor engages, and `sees` hides the ground above a cliff from those below who cannot leap it.
- The live tier holds 5,000 people under a capacity of 6,250 (#216, pull request #233). The live scenario's tick is a target of 4 ms p99; the separation pass, the hold before a moving foe and the engage search each take about a third of it.

### Options weighed for the slots

- **A ring of slots per foe, sized by the striker's reach, each spaced at least the separation apart.** Chosen.
  - The ring's radius is the reach less two steps (32 units), so a striker on its slot stays in reach while separation jostles it.
  - It holds the most slots, up to 16, whose neighbours lie at least `SEPARATION` apart. For the basic strike that is a radius of 352 units and 8 slots.
  - Slot `k` of `n` lies at angle `k / n` of a turn from east, as an offset from a fixed integer table built once per ring size and pinned by a test. No floating point is read at run time.
- **The 8 cells around the foe.** Rejected: the diagonal cells' centres lie 1.41 cells from the foe, 23 units inside the basic strike's reach, so a striker jostled there drifts out of reach. Neighbouring cells also sit exactly the separation apart, where its push begins. A longer reach would still be held to arm's length.
- **No slots, only tokens.** Rejected: waiting people would still close on one point and pile up behind the token holders, which is the look the owner wants gone.

### Options weighed for assigning slots

- **Sticky, in a fixed order.**
  - Each tick, the people engaging a foe are taken in order of distance squared, then person index, positions read as the tick began.
  - A person keeps the slot it holds while it engages that foe. A person without a slot takes the free slot nearest it, the lower slot index among equals.
  - Chosen. The order is a total order on world state, so the same world gives the same slots on every platform and draws nothing from the seeded streams.
- **Reassign everyone each tick by distance.** Rejected: two people near the boundary between slots swap back and forth as they step, so slots flicker and people dither.

### Options weighed for the tokens

- **Tokens per foe by the foe's realm rank, from a table in the rules (pack data under ADR 0049).** Chosen.
  - The region's default is 2 tokens for the two lowest ranks, 3 for the next two, 4 for the next two, and 6 above. A master draws more attackers at once, and a novice is not swarmed.
  - The table may grow a column by size once bodies have sizes; nothing has one yet.
- **One fixed count for everyone.** Rejected: the owner suggested realm or size, and a fixed count makes a master as easy to swarm as a levy.
- **Tokens by the attacker.** Rejected: the pattern is the defender's: how many may set on one person at once.

### Options weighed for what a person without a token does

- **It holds on a waiting ring outside the foe's reach and circles, until a token frees.** Chosen.
  - The waiting ring's radius is the striker's reach plus the separation plus two steps: 672 units, 2.6 cells, for the basic strike. Its slots are spaced by the same rule, up to 16.
  - A waiter takes a waiting slot as an attacker takes a slot. Every 30 ticks it moves on to the next free waiting slot, clockwise for an even person index and counter-clockwise for an odd one, so a ring of waiters circles in both directions.
  - Waiters queue in the order they began to wait, then by person index. When a token frees, the head of the queue takes it and walks to the slot nearest it.
  - A waiter beyond the waiting ring's slots holds where it stands, outside reach.
- **It looks for another foe.** Partly chosen: a person engaging on its own, with no attack intent, takes the nearest foe in sight that has a free token or has granted it one, through the same outward search. Only when none has one does it wait on the nearest. A crowd then spreads over a line of foes before it queues on one. A person with an attack intent waits on its target: its player chose it.
- **It stands where it is.** Rejected: a crowd of waiters then stands in a clump behind the holders, which is the pile-up again.

### Options weighed for what tokens gate

- **A use aimed at a person needs that person's token: the automatic strike and a cast at a person. A use aimed at a point does not, and hits whoever its shape covers.** Chosen.
  - Area, arc and line uses then catch people in a crowd as ADR 0046 resolves them.
  - Only the choice of whom to aim at is limited, so one striker cannot be ganged on.
  - A cast at a person without its token is refused with a reason, `NO_TOKEN`, as the cast port refuses other casts.
- **Every use near a foe needs a token.** Rejected: an area art swept through a crowd would need a token from each person it covers, which is not what tokens model.

### Options weighed for freeing

- **A slot and a token free on the tick their holder stops engaging the foe.** Chosen. That is when:
  - the holder or the foe dies;
  - the holder chooses another foe, or its attack intent ends or moves (ADR 0066);
  - the holder starts a leap (ADR 0058);
  - the holder lies beyond the waiting ring at the tick's start, which counts as flight.

  A foe's death frees all its slots and tokens.
- **Leases.** A holder that has held a token for 90 ticks gives it up while someone waits, and joins the back of the queue. The lease is rules data, 0 for none. Without it, the first two to reach a master would strike it until one died while the rest circled.

### How it fits what exists

- **ADR 0066.** An intent's target is the foe its members engage. The members close on their slots, not on the target, and strike it only with its token. Waiting with an intent keeps the intent. Everything that ends an intent frees its slots and tokens.
- **ADR 0046.**
  - The engage search keeps its sight and claim rules and gains the free-token preference above.
  - A person engaging steps toward its slot or waiting slot by the same open step, an intent's chase keeping its axis step around a shut corner. It stands to fight while on its slot, within a quarter cell, with the token.
  - Shapes are unchanged.
- **ADR 0037 and ADR 0017.**
  - The flow field still takes an army to its objective; slots act only through the engage pull, within the rules' sight, so no field is built for a slot.
  - Separation is unchanged. Slots and waiting slots lie at least the separation apart, so the push and the slots agree.
  - The hold before a moving foe reads the striker's reach, and the waiting ring lies outside it, so waiters are not held by the foe.
- **ADR 0058.** A slot or waiting slot is offered only on a passable cell whose level is within a step of the foe's cell, so nobody is sent to a slot it would have to leap to. A leaping person takes and holds none.
- **Duels.** A duel's two fighters take each other's first token before anyone else does, so a duel is never refused for want of one.
- **The player.** The rule is the same: people attacking the player need its tokens, and the player's strikes and casts at a person need that person's. The HUD shows the reason when a cast is refused for `NO_TOKEN`.

### Cost

State per person, in structure-of-arrays form sized to the world's capacity at construction and on load, never resized on a tick:
- the engaged foe as a 4-byte index;
- the slot or waiting slot as 1 byte;
- flags (token held, waiting) as 1 byte;
- the tick waiting began and the tick the token was taken, 4 bytes each.

That is 14 bytes a person.

State per foe, indexed by person:
- a 16-bit mask of the slots taken;
- a 16-bit mask of the waiting slots taken;
- the tokens held as 1 byte.

That is 5 bytes a person.

Totals: 19 bytes a person, 9.5 KB at 500 and 95 KB at 5,000, 119 KB at a capacity of 6,250.

The tick's assignment reuses one buffer of person indices, sized to the capacity:
- A counting sort groups the people engaging by foe, in O(people).
- Each person then does at most 16 slot tests, and each freed token one queue step.

In the live scenario, 2,000 fight: about 32,000 slot tests and 2,000 sort steps, under 0.1 ms. The free-token preference can make the engage search look past the nearest foe. The search already walks outward and asks `accept` only of a nearer candidate, so it is one search, not two, and each extra candidate it rejects costs one check. Nothing allocates on a tick.

The state is world state: saved, and hashed only when someone holds a slot. `SAVE_VERSION` rises, and the pinned crossing hash changes once, in the step that first fills slots.

## Decision

Combat gains attack slots and attack tokens, the same for every person.

**Slots.** Around each foe, the people engaging it are given slots on a ring sized by their reach, spaced at least the separation apart, up to 16. They are assigned each tick in order of distance squared, then person index, and kept while held. Each person closes on its slot, not on the foe.

**Tokens.** Each foe grants tokens by its realm rank from a table in the rules, by default 2, 2, 3, 3, 4, 4 and 6 above. Only a token holder may aim a use at that person, whether an automatic strike or a cast; a cast without it is refused with `NO_TOKEN`. A use aimed at a point needs no token.

**Waiting.** A person without a token waits on a ring outside reach and circles it every 30 ticks, queued by when it began to wait, then by index. A person engaging on its own prefers a foe in sight with a free token.

**Freeing.** Slots and tokens free when the holder dies, chooses another foe, ends or moves its intent, starts a leap, or lies beyond the waiting ring; when the foe dies; and after a lease of 90 ticks while someone waits.

**Where it fits.**
- ADR 0066's intent: members close on slots around its target and strike it with its tokens.
- ADR 0046's engage rule: steps toward a slot instead of the foe.
- ADR 0058: offers no slot a leap away.

The flow field, separation, shapes and duels keep their rules.

**What stays fixed.** The slot geometry, the ring rules and the order of assignment are code. The token table, the circling period and the lease are rules data.

**Implementation steps.** Each step is its own pull request, with `make ci`, GCC, `make sweep` keeping 48 of 50 seeds or better, and `make bench` passing on AC power:
1. **State and data, no behaviour.**
   - The per-person and per-foe arrays, the token table, the circling period and the lease in the rules, saved and hashed only when set.
   - Tests: a save round trip, and a world with no slots hashing as before.
   - Sweep and bench identical.
2. **Slots.**
   - People engaging close on slots; the tokens are unbounded in this step, so who strikes is unchanged.
   - Tests:
     - eight attackers around one foe stand on eight distinct slots at least the separation apart; control: with a slot count of 0 in the test's rules, they close straight on, inside the separation of one another;
     - the same slots whatever the order the armies were listed in;
     - no slot on a blocked cell, or a level more than a step from the foe's.
   - The crossing hash re-pinned once, with the reason.
3. **Tokens.**
   - Gating, the waiting ring and circling, freeing, leases, the free-token preference and `NO_TOKEN`.
   - Tests:
     - eight against one: at most the foe's tokens strike it on any tick; control: with tokens at 8, all eight strike;
     - a holder's death gives the head of the queue the token within a tick, and it strikes once in reach;
     - a retarget, an intent ended and a flight beyond the waiting ring each free the token;
     - a waiter's waiting slot moves on every 30 ticks;
     - an area use still hits a waiter in its shape;
     - the player as a foe is capped by its own tokens;
     - a cast at a person without its token is refused with `NO_TOKEN`.
   - Retune the token table if the sweep falls below 48 of 50; the M1 bench's floor of 16 strikes a tick is checked.
4. **The view.**
   - Nothing new to decide. The waiting ring reads from the people's positions.
   - Captures: one foe ringed by a crowd, before step 2 and after step 3.

## Consequences

- A crowd attacking one person surrounds it on a ring and strikes it two to six at a time. The others circle outside, so one against many lasts longer and reads as a brawl.
- Fights end later, by up to the ratio of attackers to tokens, so raids, duels and wars may miss their target times until the token table is tuned. The sweep watches that.
- Engaging people spread over a line of foes rather than all striking the nearest, since a foe with no free token sends them to the next.
- Area arts gain value against crowds: they ignore tokens.
- The player gets the same protection in a crowd, and needs a token to strike a foe already engaged by allies, which can mean waiting.
- The tick gains 19 bytes a person and well under a tenth of a millisecond of work at 5,000. Saves grow by the engaged people's state.

## Signs the decision was wrong

- The player waits for a token on a foe its allies hold, and finds it frustrating enough that the steered body needs to come first.
- Battles of two lines slow until the sweep falls below 48 of 50 at any token table, or the M1 bench falls below 16 strikes a tick.
- Waiters circling block movers passing by, because the waiting ring cuts across a road or a gate.
- The free-token preference makes people walk past a near foe to a far one, and lines stretch thin instead of meeting.
- Slots jitter: people step back and forth between a slot and its neighbour often enough to show.
