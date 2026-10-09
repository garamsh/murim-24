# Region screens

The first four of #200's in-game screens per region, in the look of `../ground-shader/`: Central Plains (중원), Sichuan (사천), the Western Regions (서역) and Jiangnan (강남). Each region's landmarks come from the PM's map plan.

- **How they are made:** `angle.gd` and `ground.gdshader` here, the same files as `art/mockup/angle.gd` and `art/mockup/angle/ground.gdshader` at ed38205, run with Godot 4.7.2 on macOS arm64 (Apple M1).
- **Command:** `godot --path . --script res://angle.gd -- <dir> stills <region>`, with region one of `central`, `sichuan`, `west` and `jiangnan`.
- **Same path as the valley:** each region is laid out by hand on the valley's 128 x 96 grid. It is drawn by the same ground shader, map-build bake, streaming and HUD as the valley. 80 people from the core walk its lanes for 90 ticks before the capture.

## Files

- `<region>_ko.png`: the region at 2x under the HUD, in Korean.
- `<region>_village_ko.png`: its town or village at the PC's 3x.
- `sprites/`: the new sheets at 3x.
- `angle.gd`, `ground.gdshader`: the scripts that made them.

## The regions

### Central Plains (`central`)

- Wheat fields to the horizon, laid as a patchwork of fields between green balks, with fallow and vegetable plots among them.
- The Yellow River across the north, muddy and streaked with its current. It has silt beaches and willows on its south bank, and bends north under the mountain.
- Luoyang's south wall, gate and watchtowers, with its streets, yamen, inn, stalls and lanterns behind them. A farming hamlet with stacks and a cart stands on the road east.
- Song Shan standing alone, with pines and a peak. Shaolin's terrace sits two levels up its south face: a hall, two pagodas, a shrine and a pavilion behind a low wall. Its stair leads down to the mountain gate and its guardians. Three pagodas of the monks' graves stand at the foot.
- 3x: Luoyang's gate and streets.

### Sichuan (`sichuan`)

- A basin ringed by forested peaks, with mist lying in its low ground.
- Paddies on terraces climb its north side, with houses and a stair.
- A river runs out of a gorge cut through the eastern range, under a tall rock cliff, and south across the basin.
- Bamboo and broadleaf groves fill the basin.
- The Tang clan's walled compound stands on the floor: a main hall, side halls, houses, crocks of its poisons and watchtowers, with bamboo crowding its walls. A village of thatched and tiled houses lies by the river among the paddies.
- 3x: the Tang clan's compound.

### Western Regions (`west`)

- Sand and tan hardpan under the Tianshan, whose tiers above the second level lie under snow. Snowy pines grow on the range.
- The Cult's fortress on a terrace four levels up the range: its gate tower in a stone wall, the Cult's black and crimson hall, side halls, watchtowers and braziers. A stair runs down a rounded rock landing to the sand.
- The wall of Jiayu Pass across the corridor, with its gate tower and watchtowers.
- An oasis south of the pass: a pool among grass, poplars and willows, adobe houses round it and a caravan's carts.
- Dunes run away east, rising as slopes and lit on their western faces.
- 3x: the pass and the oasis.

### Jiangnan (`jiangnan`)

- A water town of whitewashed houses with dark roofs and horse-head gables, some with a timber upper storey on the water.
- A grid of canals faced in stone, with streets along them and grass courts between the rows. Arched stone bridges cross the canals, with footbridges, boats, stalls, lanterns and willows.
- Tai Lake's shore to the west, tea on low hills to the north-east, and paddies with a hamlet south of the town.
- 3x: the town's canals and bridges.

## What was added for them

- **Ground shader:**
  - three kinds: sand with wind ripples, snow, and wheat in rows;
  - `mist` in the low ground, dithered in clusters, not a checker;
  - `moss`, so dry and snowy rock wears none;
  - `relief`, lighting slopes as they face, so dunes and gentle hills show their shape;
  - `flow`, streaking rivers and canals along their current;
  - snow or sand hanging over a cliff's lip where that is the ground above.
  - All are off for the valley.
- **View:** a look may now recolour kinds of ground, set the shader's air, and name its own trees.
- **Valley:** a region name lays out a region in place of the valley. The valley's own layout is unchanged: its ten stills at ed38205 are byte-identical to 8440a71's.
- **Art** (`buildings.lua`, `terrain.lua`, `mountains.lua`; all colours from the master palette):
  - `water_house` and `water_house_big`: Jiangnan's whitewashed houses;
  - `arch_bridge`: a stone hump bridge over a canal;
  - `adobe_house` and `adobe_small`: flat-roofed adobe houses;
  - `pagoda`: a seven-storey brick pagoda;
  - `cult_hall`: the Cult's hall;
  - trees `poplar` and `snow_pine`;
  - peaks tagged `snow`.
  - Rerunning `art/mockup/build.sh` leaves `git status` clean.

## Frame cost

Not measured: the machine was never quiet (load < 4, no other test or build running) in the hour the capped runs waited, and the port (#200) is to be measured instead.

## Not done

- **Map plan:** the regions are composed at the scale of one screen, not placed where the map plan puts them. Each stands alone on its own grid, as the valley does.
- **Minimap:** the HUD's minimap shows the generated region the HUD runs on, not the region drawn, as in the valley's stills.
- **Locale:** captures are in Korean only, as asked.
- **Straight edges left:** the terraces of Shaolin and of the Cult's fortress are rectangles, as built ground. Jiangnan's town is a regular grid of rows along the canals.
