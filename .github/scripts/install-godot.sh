#!/bin/sh
# Installs the Godot 4.7.2 Linux x86_64 editor into the directory given, as `godot`, after
# checking the release archive against the SHA-512 in the release's SHA512-SUMS.txt.
# chickensoft-games/setup-godot was not used: it downloads by version and checks no hash.
set -eu

dir=$1
version=4.7.2
archive=Godot_v$version-stable_linux.x86_64
sha512=9aa00f7a605200940bce3027a567b782f49bd8e940dd06ae9e987bd65aee1b1467edd56ed84fcdcbdd44354bf613bdbb4e5d2913e925850368e150c59ed54c65

mkdir -p "$dir"
curl -sSfL -o "$dir/$archive.zip" "https://github.com/godotengine/godot/releases/download/$version-stable/$archive.zip"
echo "$sha512  $dir/$archive.zip" | sha512sum -c -
unzip -q "$dir/$archive.zip" -d "$dir"
mv "$dir/$archive" "$dir/godot"
rm "$dir/$archive.zip"
echo "installed Godot $version, its archive checked against SHA-512, as $dir/godot"
