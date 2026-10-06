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
"$aseprite" --batch --script markers/markers.lua

for name in terrain/terrain terrain/transitions terrain/landforms bodies/orthodox bodies/demonic \
	bodies/unorthodox structures/house structures/hall markers/army_marker markers/player_marker; do
	"$aseprite" --batch "$name.aseprite" --sheet "$name.png" --data "$name.json" --format json-array --list-tags
done

"$aseprite" --batch --script contact_sheet/contact_sheet.lua
"$aseprite" --batch contact_sheet/contact_sheet.aseprite --save-as contact_sheet/contact_sheet.png
"$aseprite" --batch contact_sheet/contact_sheet.aseprite --scale 4 --save-as contact_sheet/contact_sheet_4x.png
