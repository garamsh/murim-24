#!/bin/zsh
# Mutations for #184's build: name, file, perl substitution, tests. C++ files rebuild before and after.
cd "$(dirname "$0")/../.."
L=build/issue355/logs
mut() {
  local name=$1 file=$2 sub=$3; shift 3
  cp "$file" /tmp/m355.bak
  perl -0pi -e "$sub" "$file"
  if cmp -s "$file" /tmp/m355.bak; then echo "$name: MUTATION DID NOT APPLY"; cp /tmp/m355.bak "$file"; return; fi
  case $file in *.cpp|*.hpp) make build > $L/mut-build.log 2>&1 || { echo "$name: BUILD FAILED"; cp /tmp/m355.bak "$file"; make build >/dev/null 2>&1; return; } ;; esac
  godot --headless --path . --script res://run_tests.gd -- "$@" > $L/mut-$name.log 2>&1
  echo "$name: exit $? $(grep -cE ': FAIL$' $L/mut-$name.log) FAIL lines, $(grep -c 'SCRIPT ERROR' $L/mut-$name.log) script errors"
  cp /tmp/m355.bak "$file"
  case $file in *.cpp|*.hpp) make build >/dev/null 2>&1 ;; esac
}
mut every-use-shaped extension/body_view.cpp 's/  if \(strike.steered\) \{\n    return true;\n  \}\n  return std::any_of/  if (strike.steered || true) {\n    return true;\n  }\n  return std::any_of/' res://extension/body_view_test.gd
mut target-ignored extension/body_view.cpp 's/\(target_ >= 0 && person == target_\)/(target_ >= 0 \&\& person == target_ \&\& false)/' res://extension/body_view_test.gd
mut struck-player-ignored extension/body_view.cpp 's/return person == lead_ \|\|/return (person == lead_ \&\& false) ||/' res://extension/body_view_test.gd
