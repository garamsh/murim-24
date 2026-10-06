# Evidence for PR #101

Captures of fix/buildable-terrain at 25de7be2a23a8c5c668853e714054500753e5240, the PR's head. This branch is never merged. Each PNG is the frame its log line below names. The eleven files have different SHA-256 hashes, and each script fails if a capture repeats an earlier one.

## Generated regions

region_24.png, region_2026.png and region_7.png are each seed's whole region, 128 x 128 cells at one 16-pixel tile per cell, drawn through world_art.tscn: terrain, landforms, villages and bodies. Forest is the dark green, hills are the light bushes, and mountains are grey. No structure stands on hills or mountains. capture_region.gd prints each kind's share of the cells and the hash of the image data:

    capture_region: region_24.png, 6 villages, 44 structures, sha256 0235de2aaa9cf21b, cells plain 71%, forest 12%, hills 4%, water 5%, mountain 7%
    capture_region: region_2026.png, 7 villages, 74 structures, sha256 c21e79668c5df505, cells plain 62%, forest 16%, hills 17%, water 4%, mountain 1%
    capture_region: region_7.png, 7 villages, 82 structures, sha256 e8686c63dd6a273e, cells plain 48%, forest 19%, hills 25%, water 4%, mountain 4%

## Grown villages

The scene is play.tscn on seed 2026 with the player born in village 0. The script drives the ticks itself at play pace, 1,800 ticks a day, and holds the camera on what it captures. Since #98 the body bands draw after a camera jump, so this script no longer recreates them, unlike PR #90's.

- growth_founded_laid.png, growth_founded_raised.png (and their _4x copies): the region's first founded village, laid as rubble and then raised by its settlers, on plain.
- growth_built_laid.png, growth_built_raised.png (and their _4x copies): the first single building laid outside a settlement, a granary, and then raised. When the capture landed it had already been mended from rubble to the destroyed stage (2).

capture_growth.gd prints the tick, the date, and each structure's kind, cell and stage (0 intact, 2 destroyed, 3 rubble):

    capture_growth: settlement laid at tick 1457996, date { "year": 7, "season": 3, "day": 1 }, settlers 10 in army 8
    capture_growth: growth_founded_laid.png at tick 1457999, date { "year": 7, "season": 3, "day": 1 }, structures (kind, cell, stage) ["house (64, 12) 3", "house (68, 12) 3", "house (66, 18) 3", "barrels (70, 15) 3", "field (69, 18) 3", "granary (64, 18) 3"]
    capture_growth: growth_founded_raised.png at tick 1464548, date { "year": 7, "season": 3, "day": 4 }, structures (kind, cell, stage) ["house (64, 12) 0", "house (68, 12) 0", "house (66, 18) 0", "barrels (70, 15) 0", "field (69, 18) 0", "granary (64, 18) 0"]
    capture_growth: growth_built_laid.png at tick 1565999, date { "year": 8, "season": 1, "day": 1 }, structures (kind, cell, stage) ["granary (87, 5) 2"]
    capture_growth: growth_built_raised.png at tick 1576314, date { "year": 8, "season": 1, "day": 6 }, structures (kind, cell, stage) ["granary (87, 5) 0"]

## File hashes

SHA-256 of each file, first 16 hex digits:

    7b75d9cf44a7568a  growth_built_laid_4x.png
    581b799875f5e7f1  growth_built_laid.png
    66ccd03a360d71f7  growth_built_raised_4x.png
    63cf55cf7d55cbbb  growth_built_raised.png
    a57b5d123975c1ca  growth_founded_laid_4x.png
    e5d362a051fd923e  growth_founded_laid.png
    adcfb3590376010f  growth_founded_raised_4x.png
    0cd450cdbb29c855  growth_founded_raised.png
    faab0135ea5806f4  region_2026.png
    a0d19079787b901f  region_24.png
    b53cfdc325e09fc1  region_7.png

## Commands

Run from the project root after make build:

    godot --path . --script capture_region.gd -- <dir> 24 2026 7
    godot --path . --script capture_growth.gd -- <dir>
    godot --headless --script upscale.gd -- <png>...
