#!/bin/sh
# Redraws the mockup sources from their scripts and exports each at native size and at 4x, then
# redraws and exports the 2.5D angle's sheets under angle/.
# Run from anywhere: art/mockup/build.sh [path to aseprite]
set -eu

aseprite=${1:-aseprite}
cd "$(dirname "$0")"

for name in target_screen lineup_sheet genealogy_panel sect_panel title_screen new_game_screen pause_screen settings_screen \
	mobile_hud_idle mobile_hud_aim mobile_hud_cancel mobile_hud_self mobile_hud_targets arts_panel arts_panel_touch \
	mobile_hud_arts; do
	"$aseprite" --batch --script "$name.lua"
	"$aseprite" --batch "$name.aseprite" --save-as "$name.png"
	"$aseprite" --batch "$name.aseprite" --scale 4 --save-as "${name}_4x.png"
done

# The 2.5D angle of #200: sheets with their frames and tags, as the production set is exported.
for script in terrain structures bodies effects; do
	"$aseprite" --batch --script "angle/$script.lua"
done
for name in terrain faces trees house hall storehouse manual_hall granary well seat_gate barrels fence \
	training_ground orthodox demonic marks qi pillar dust debris; do
	"$aseprite" --batch "angle/$name.aseprite" --sheet "angle/$name.png" --data "angle/$name.json" --format json-array \
		--list-tags
done
