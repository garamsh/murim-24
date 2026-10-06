#!/bin/sh
# Redraws the mockup sources from their scripts and exports each at native size and at 4x.
# Run from anywhere: art/mockup/build.sh [path to aseprite]
set -eu

aseprite=${1:-aseprite}
cd "$(dirname "$0")"

for name in target_screen lineup_sheet genealogy_panel; do
	"$aseprite" --batch --script "$name.lua"
	"$aseprite" --batch "$name.aseprite" --save-as "$name.png"
	"$aseprite" --batch "$name.aseprite" --scale 4 --save-as "${name}_4x.png"
done
