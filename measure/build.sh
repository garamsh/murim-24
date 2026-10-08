#!/bin/sh
# Builds the measure program against core/ at a checkout of murim-24 (origin/main 1d943f0),
# with MAX_PEOPLE and REGION_SIDE made settable by raise-bounds.patch. Usage: build.sh <repo>
set -eu
repo=$1
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cp "$repo"/core/*.cpp "$repo"/core/*.hpp "$work"/
rm -f "$work"/*_test.cpp "$work"/bench.cpp "$work"/sweep.cpp
(cd "$work" && mkdir -p core && mv ./*.hpp ./*.cpp core/ && patch -p1 < "$here/raise-bounds.patch" && mv core/* . && rmdir core)
cp "$here/measure.cpp" "$work"/
for side in 128 512; do
  clang++ -std=c++17 -O3 -DNDEBUG -DMEASURE_MAX_PEOPLE=5000 -DMEASURE_REGION_SIDE=$side -I"$work" "$work"/*.cpp -o "$here/measure_$side"
done
echo "built $here/measure_128 and $here/measure_512"
