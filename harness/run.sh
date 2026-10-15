#!/bin/sh
# Runs every capture in this branch on one tree, built with `make build`. Usage:
#   run.sh <tree> <tag> <out>
# <tag> is "before" (main cbd37d2f, the parent of d16771b) or "after" (main b2556de).
set -eu
tree=$1 tag=$2 out=$3
here=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$out/m1" "$out/ring" "$out/logs"
cd "$tree"
TAG=$tag OUT=$out godot --path . --fixed-fps 30 --script "$here/capture.gd" > "$out/logs/capture-$tag.log" 2>&1
for shot in p74:17550,12530:1440 p309:17150,17780:1050 crowded:crowded:1440 crowded:crowded:1050; do
	label=${shot%%:*} rest=${shot#*:} at=${rest%%:*} tick=${rest#*:}
	TAG=$tag LABEL=$label AT=$at SHOT=$tick OUT=$out godot --path . --fixed-fps 30 --script "$here/ring.gd" \
		> "$out/logs/ring-$tag-$label-$tick.log" 2>&1
done
