# Audio

How Murim24 sounds: each sound and what plays it, where every file comes from and under what licence, and how they are mixed.

## Sounds

The sound scene, `sound/sound.tscn`, plays what the core reports and holds no game rule (`docs/architecture/godot-adapter.md`).

| File | Plays when | What it is |
|---|---|---|
| `sound/strike.wav` | A body strikes: its ready tick moves | A blade's swish, noise swept up through a low-pass |
| `sound/hit.wav` | A living body's health falls | A thud, a tone falling from 150 to 55 Hz under a click |
| `sound/fall.wav` | The core records a death | A body falling, two low thumps |
| `sound/fire.wav` | A structure falls a stage | A torch catching, a rising roar with crackles |
| `sound/mend.wav` | A structure rises a stage | Building, two wooden knocks |
| `sound/bell.wav` | The core records a raid sent against a village | The village's bell, struck twice |
| `sound/click.wav` | A screen takes a key | A short tick |
| `sound/open.wav` | A screen or panel opens | A paper rustle and a plucked note |
| `sound/explore.wav` | No strike was heard in the last 8 seconds | A slow pentatonic melody on a plucked string over a drone, a gong every four bars; loops |
| `sound/battle.wav` | A strike was heard in the last 8 seconds | A faster minor-pentatonic melody over drums, a gong every eight bars; loops |

## Sources and licence

Every file in the table is made here. `sound/generate.gd` synthesises each from the parameters it holds, drawing every random value from a seeded generator, so a run writes the same files. The WAVs are committed beside it, and so is the `.import` file Godot writes beside each; the music's set it to loop.

Run: `godot --headless --path . --script res://sound/generate.gd`

| File | Source | Licence | Author |
|---|---|---|---|
| `sound/strike.wav`, `hit.wav`, `fall.wav`, `fire.wav`, `mend.wav`, `bell.wav`, `click.wav`, `open.wav` | `sound/generate.gd`, `EFFECTS` | The project's own | Murim24, made here |
| `sound/explore.wav`, `battle.wav` | `sound/generate.gd`, `MUSIC` | The project's own | Murim24, made here |

- A file made here is the project's own, under no licence dedication. What licence the project's own work takes is undecided: the repository has no licence file.
- A file from outside is added only under CC0, with its source URL, licence and author in this table.

## Mixing

- Three audio buses, in `default_bus_layout.tres`: Master, and Effects and Music sending to it. The settings panel sets each bus's volume in quarters, none muting it.
- Effects sound where they happened, through a fixed pool of 12 voices, and only within the part of the world on screen. At most one of each kind starts a tick; once every voice sounds, what finds none is dropped, so a battle of 500 people sounds as many voices as one of ten.
- The UI's click and opening sound through a voice of their own on the Effects bus, outside the pool.
- Music plays on the Music bus, one track at a time, starting over when it changes.
