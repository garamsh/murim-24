#!/bin/sh
# Redraws #170's trade-looks mockup from art/mockup/trades.lua and exports it: each trade's sheet
# with its frames and tags as the production set is exported, the contact sheet at native size and
# 4x, and every frame at 6x.
# Run from anywhere: art/mockup/trades.sh [path to aseprite]
set -eu

aseprite=${1:-aseprite}
cd "$(dirname "$0")"

"$aseprite" --batch --script trades.lua
for name in merchant physician smith farmer innkeeper beggar monk; do
	"$aseprite" --batch "trades/$name.aseprite" --sheet "trades/$name.png" --data "trades/$name.json" \
		--format json-array --list-tags
done
"$aseprite" --batch trades/trades_sheet.aseprite --save-as trades/trades_sheet.png
"$aseprite" --batch trades/trades_sheet.aseprite --scale 4 --save-as trades/trades_sheet_4x.png
"$aseprite" --batch trades/trades_frames.aseprite --scale 6 --save-as trades/trades_frames_6x.png
