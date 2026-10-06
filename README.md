# Evidence for PR #90

Captures of feat/expansion at 0a976986d4673ee8331ceadc5845a487b96dfb77, the PR's head. This branch is never merged. Each PNG is the frame its log line below names; the four have different hashes, and the script fails if a capture repeats an earlier one.

The scene is play.tscn on seed 2026 with the player born in village 0. The script drives the ticks itself at play pace, 1,800 ticks a day, and holds the camera on what it captures.

- growth_founded_laid.png, growth_founded_raised.png (and their _4x copies): the region's first founded village.
  - laid: tick 809,999, year 4, the first day of winter. A pressed village has laid out a hamlet as rubble: three houses, barrels, a granary and a field, burnt-looking in winter. Its 10 settlers are still walking there, about 17 cells east, off screen.
  - raised: tick 816,923, three days later. The settlers have raised every structure by their mend, the repair rule, and stand in the new village's square.
- growth_built_laid.png, growth_built_raised.png (and their _4x copies): the first single building laid outside a settlement.
  - laid: tick 917,999, year 5, summer. A pressed village has laid a granary as rubble east of its square.
  - raised: tick 928,329, about six days later. Its residents have raised it intact.

The script recreates world_art's body bands whenever the camera lands. Bands filled at the player's village drew no body after the camera jumped across the region, although world_art counted them as drawn. PR #90 notes this; it does not change that code.

capture_growth.gd prints the tick, the date, and each structure's kind, cell and stage (0 intact, 3 rubble):

    capture_growth: settlement laid at tick 809996, date { "year": 4, "season": 3, "day": 1 }, settlers 10 in army 8
    capture_growth: growth_founded_laid.png at tick 809999, date { "year": 4, "season": 3, "day": 1 }, structures (kind, cell, stage) ["house (91, 84) 3", "house (95, 84) 3", "house (93, 90) 3", "barrels (97, 87) 3", "field (96, 90) 3", "granary (91, 90) 3"]
    capture_growth: growth_founded_raised.png at tick 816923, date { "year": 4, "season": 3, "day": 4 }, structures (kind, cell, stage) ["house (91, 84) 0", "house (95, 84) 0", "house (93, 90) 0", "barrels (97, 87) 0", "field (96, 90) 0", "granary (91, 90) 0"]
    capture_growth: growth_built_laid.png at tick 917999, date { "year": 5, "season": 1, "day": 1 }, structures (kind, cell, stage) ["granary (116, 57) 3"]
    capture_growth: growth_built_raised.png at tick 928329, date { "year": 5, "season": 1, "day": 6 }, structures (kind, cell, stage) ["granary (116, 57) 0"]

Commands, from the project root after make build:

    godot --path . --script capture_growth.gd -- <dir>
    godot --headless --script upscale.gd -- <png>...
