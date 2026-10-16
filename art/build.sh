#!/bin/sh
# Redraws every production asset from its script, exports each with the command in
# docs/convention/stack-godot.md §Asset export, then draws the contact sheet from the
# exported sources. The mockups under mockup/ have their own build.sh.
# Run from anywhere: art/build.sh [path to aseprite]
set -eu

aseprite=${1:-aseprite}
cd "$(dirname "$0")"

"$aseprite" --batch --script terrain/trees.lua
"$aseprite" --batch --script terrain/ground.lua
"$aseprite" --batch --script bodies/bodies.lua
"$aseprite" --batch --script effects/effects.lua
"$aseprite" --batch --script structures/structures.lua
"$aseprite" --batch --script structures/workshops.lua
"$aseprite" --batch --script decorations/decorations.lua
"$aseprite" --batch --script decorations/fields.lua
"$aseprite" --batch --script icons/icons.lua
"$aseprite" --batch --script markers/markers.lua
"$aseprite" --batch --script title/backdrop.lua
"$aseprite" --batch --script icon/icon.lua

for name in terrain/trees terrain/ramps terrain/detail bodies/orthodox bodies/demonic \
	bodies/unorthodox effects/attack structures/house structures/hall structures/well structures/barrels structures/fence \
	structures/granary structures/storehouse structures/training_ground structures/manual_hall structures/temple structures/inn \
	structures/seat_gate structures/smithy structures/apothecary structures/town_wall_south structures/town_wall_north \
	structures/town_wall_west structures/town_wall_east structures/town_corner_tower structures/town_gate_tower \
	structures/town_steps structures/town_temple structures/town_market_stall structures/town_courtyard_house \
	decorations/fields icons/economy_icons \
	decorations/ground decorations/banner decorations/sect_banner icons/skill_icons icons/ware_icons markers/army_marker \
	markers/player_marker markers/shadow title/backdrop icon/icon; do
	"$aseprite" --batch "$name.aseprite" --sheet "$name.png" --data "$name.json" --format json-array --list-tags
done

"$aseprite" --batch --script contact_sheet/contact_sheet.lua
"$aseprite" --batch contact_sheet/contact_sheet.aseprite --save-as contact_sheet/contact_sheet.png
"$aseprite" --batch contact_sheet/contact_sheet.aseprite --scale 4 --save-as contact_sheet/contact_sheet_4x.png
