# Evidence for PR #100

These are captures of feat/live-territory at 0874af988fb11190ca7d6eb3ba44ff8225b351d2, the PR's head. This branch is never merged. Each PNG is the frame its log line below names. The five frames have different hashes, and the script fails if a frame repeats an earlier one.

The scene is play.tscn on seed 2026, with the player born in village 0. A second world of the same seed runs ahead first to find the first founding: hamlet 8, at corner (91, 84), on tick 810,000. The script drives the ticks itself, 1,800 to a day, running the last tick of each stretch through play.step as play.gd does. It holds the camera on what it captures and centres the HUD's map there instead of on the player.

- village_before.png: tick 809,999, the last tick before the founding. The camera is on the cell where the hamlet will stand, and nothing is there.
- village_founded.png: tick 810,000. The hamlet's layout is laid as rubble. The same template drawing that draws the generated hamlets lays its yellow Unorthodox banner and its road. The hamlet template has no paving in `world_art/villages.json`. On the map, the new village's square and its claim's border have appeared.
- village_raised.png: tick 816,925, three days later. The settlers have raised every structure and stand in the square.
- border_before.png: tick 809,999. The camera is on the nearest cell that becomes a border cell at the founding: (83, 98), at the edge of the new claim, 16 cells from the hamlet's centre. No border is drawn there.
- border_founded.png: tick 810,000. The dotted Unorthodox border now runs along the new claim's edge, on the claimed side only, since the land beyond it lies in no territory. The map shows the same border.

Each frame also has a _4x nearest-neighbour copy.

capture_territory.gd prints, for each frame:
- the tick, the date and the camera's cell
- the SHA-256 of the frame's pixel data (not of the PNG file)
- the villages, and the cells whose territory changed at that moment
- the borders drawn, and whether they were drawn from the territory the core gives
- each structure of the hamlet's layout as kind, cell and stage (0 intact, 3 rubble)

    capture_territory: the scout found { "name": "hamlet 8", "kind": "hamlet", "faction": 2, "corner": (91, 84), "area": (7, 9) } founded on tick 810000
    capture_territory: village_before.png at tick 809999, date { "year": 4, "season": 2, "day": 30 }, camera on cell (94.0, 88.0), frame sha256 57bf1fc3d2e2d2d5; 7 villages, the last village 7; 0 cells changed territory at this moment; 799 borders drawn, from a territory equal to the core's true; layout (kind, cell, stage) []
    capture_territory: border_before.png at tick 809999, date { "year": 4, "season": 2, "day": 30 }, camera on cell (83.0, 98.0), frame sha256 b8020f5af17c4510; 7 villages, the last village 7; 0 cells changed territory at this moment; 799 borders drawn, from a territory equal to the core's true; layout (kind, cell, stage) []
    capture_territory: village_founded.png at tick 810000, date { "year": 4, "season": 3, "day": 1 }, camera on cell (94.0, 88.0), frame sha256 97f9901a2b126cf7; 8 villages, the last hamlet 8; 978 cells changed territory at this moment; 870 borders drawn, from a territory equal to the core's true; layout (kind, cell, stage) ["house (91, 84) 3", "house (95, 84) 3", "house (93, 90) 3", "barrels (97, 87) 3", "field (96, 90) 3", "granary (91, 90) 3"]
    capture_territory: border_founded.png at tick 810000, date { "year": 4, "season": 3, "day": 1 }, camera on cell (83.0, 98.0), frame sha256 b6ff7138f6ff3789; 8 villages, the last hamlet 8; 978 cells changed territory at this moment; 870 borders drawn, from a territory equal to the core's true; layout (kind, cell, stage) ["house (91, 84) 3", "house (95, 84) 3", "house (93, 90) 3", "barrels (97, 87) 3", "field (96, 90) 3", "granary (91, 90) 3"]
    capture_territory: village_raised.png at tick 816925, date { "year": 4, "season": 3, "day": 4 }, camera on cell (94.0, 88.0), frame sha256 ab15d3f845a9e303; 8 villages, the last hamlet 8; 0 cells changed territory at this moment; 870 borders drawn, from a territory equal to the core's true; layout (kind, cell, stage) ["house (91, 84) 0", "house (95, 84) 0", "house (93, 90) 0", "barrels (97, 87) 0", "field (96, 90) 0", "granary (91, 90) 0"]

Commands, from the project root after make build:

    godot --path . --script capture_territory.gd -- <dir>
    godot --headless --script upscale.gd -- <png>...
