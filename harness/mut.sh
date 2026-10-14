#!/bin/zsh
# Mutations for #184's build: name, file, perl substitution, tests. C++ files rebuild before and after.
cd "$(dirname "$0")/../.."
L=build/issue184/logs
mut() {
  local name=$1 file=$2 sub=$3; shift 3
  cp "$file" /tmp/m184.bak
  perl -0pi -e "$sub" "$file"
  if cmp -s "$file" /tmp/m184.bak; then echo "$name: MUTATION DID NOT APPLY"; cp /tmp/m184.bak "$file"; return; fi
  case $file in *.cpp|*.hpp) make build > $L/mut-build.log 2>&1 || { echo "$name: BUILD FAILED"; cp /tmp/m184.bak "$file"; make build >/dev/null 2>&1; return; } ;; esac
  godot --headless --path . --script res://run_tests.gd -- "$@" > $L/mut-$name.log 2>&1
  echo "$name: exit $? $(grep -cE ': FAIL$' $L/mut-$name.log) FAIL lines, $(grep -c 'SCRIPT ERROR' $L/mut-$name.log) script errors"
  cp /tmp/m184.bak "$file"
  case $file in *.cpp|*.hpp) make build >/dev/null 2>&1 ;; esac
}
mut every-shape-swings extension/body_view.cpp 's/switch \(static_cast<murim::Shape>\(strike.shape\)\) \{/switch (static_cast<murim::Shape>((strike.shape * 0) + 1)) {/' res://extension/body_view_test.gd
