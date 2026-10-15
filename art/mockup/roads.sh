#!/bin/sh
# Redraws #172's road-looks mockup from art/mockup/roads.lua and exports it: each kind's sheet and
# the caravan's with their frames and tags as the production set is exported, the contact sheet at
# native size and 4x, and every frame at 6x.
# Run from anywhere: art/mockup/roads.sh [path to aseprite]
set -eu

aseprite=${1:-aseprite}
cd "$(dirname "$0")"

"$aseprite" --batch --script roads.lua
for name in bandit_dao bandit_club bandit_axe wanderer escort_water_spear escort_water_pennant escort_stone_spear \
	escort_stone_pennant caravan; do
	"$aseprite" --batch "roads/$name.aseprite" --sheet "roads/$name.png" --data "roads/$name.json" \
		--format json-array --list-tags
done
"$aseprite" --batch roads/roads_sheet.aseprite --save-as roads/roads_sheet.png
"$aseprite" --batch roads/roads_sheet.aseprite --scale 4 --save-as roads/roads_sheet_4x.png
"$aseprite" --batch roads/roads_frames.aseprite --scale 6 --save-as roads/roads_frames_6x.png
