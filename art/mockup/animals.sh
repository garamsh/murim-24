#!/bin/sh
# Redraws #171's animal mockup from art/mockup/animals.lua and exports it: each kind's sheet with
# its frames and tags as the production set is exported, the contact sheet at native size and 4x,
# and every frame at 6x.
# Run from anywhere: art/mockup/animals.sh [path to aseprite]
set -eu

aseprite=${1:-aseprite}
cd "$(dirname "$0")"

"$aseprite" --batch --script animals.lua
for name in deer wolf boar tiger spirit_tiger_fire spirit_tiger_water; do
	"$aseprite" --batch "animals/$name.aseprite" --sheet "animals/$name.png" --data "animals/$name.json" \
		--format json-array --list-tags
done
"$aseprite" --batch animals/animals_sheet.aseprite --save-as animals/animals_sheet.png
"$aseprite" --batch animals/animals_sheet.aseprite --scale 4 --save-as animals/animals_sheet_4x.png
"$aseprite" --batch animals/animals_frames.aseprite --scale 6 --save-as animals/animals_frames_6x.png
