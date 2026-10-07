#!/bin/sh
# Redraws every production asset from its script, exports each with the command in
# docs/convention/stack-godot.md §Asset export, then draws the contact sheet from the
# exported sources. The mockups under mockup/ have their own build.sh.
# Run from anywhere: art/build.sh [path to aseprite]
set -eu

aseprite=${1:-aseprite}
cd "$(dirname "$0")"

"$aseprite" --batch --script terrain/terrain.lua
"$aseprite" --batch --script terrain/landforms.lua
"$aseprite" --batch --script bodies/bodies.lua
"$aseprite" --batch --script structures/house.lua
"$aseprite" --batch --script structures/hall.lua
"$aseprite" --batch --script structures/props.lua
"$aseprite" --batch --script structures/economy.lua
"$aseprite" --batch --script structures/seat.lua
"$aseprite" --batch --script structures/workshops.lua
"$aseprite" --batch --script decorations/decorations.lua
"$aseprite" --batch --script decorations/fields.lua
"$aseprite" --batch --script icons/icons.lua
"$aseprite" --batch --script markers/markers.lua
"$aseprite" --batch --script title/backdrop.lua
"$aseprite" --batch --script icon/icon.lua

for name in terrain/terrain terrain/transitions terrain/landforms bodies/orthodox bodies/demonic \
	bodies/unorthodox structures/house structures/hall structures/well structures/barrels structures/fence \
	structures/granary structures/storehouse structures/training_ground structures/manual_hall \
	structures/seat_gate structures/smithy structures/apothecary decorations/fields icons/economy_icons \
	decorations/ground decorations/banner decorations/sect_banner icons/skill_icons markers/army_marker \
	markers/player_marker title/backdrop icon/icon; do
	"$aseprite" --batch "$name.aseprite" --sheet "$name.png" --data "$name.json" --format json-array --list-tags
done

"$aseprite" --batch --script contact_sheet/contact_sheet.lua
"$aseprite" --batch contact_sheet/contact_sheet.aseprite --save-as contact_sheet/contact_sheet.png
"$aseprite" --batch contact_sheet/contact_sheet.aseprite --scale 4 --save-as contact_sheet/contact_sheet_4x.png
