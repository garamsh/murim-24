#!/bin/sh
# Redraws #364's temple and inn mockup from art/mockup/temple_inn.lua and exports it: each
# building's sheet with its stages as the production set is exported, the contact sheet at native
# size and 4x, every frame at 6x, and the square's layers for the night harness.
# Run from anywhere: art/mockup/temple_inn.sh [path to aseprite]
set -eu

aseprite=${1:-aseprite}
cd "$(dirname "$0")"

"$aseprite" --batch --script temple_inn.lua
for name in temple inn; do
	"$aseprite" --batch "temple_inn/$name.aseprite" --sheet "temple_inn/$name.png" --data "temple_inn/$name.json" \
		--format json-array --list-tags
done
"$aseprite" --batch temple_inn/temple_inn_sheet.aseprite --save-as temple_inn/temple_inn_sheet.png
"$aseprite" --batch temple_inn/temple_inn_sheet.aseprite --scale 4 --save-as temple_inn/temple_inn_sheet_4x.png
"$aseprite" --batch temple_inn/temple_inn_frames.aseprite --scale 6 --save-as temple_inn/temple_inn_frames_6x.png
"$aseprite" --batch temple_inn/square_ground.aseprite --save-as temple_inn/square_ground.png
"$aseprite" --batch temple_inn/square_things.aseprite --save-as temple_inn/square_things.png
