#!/bin/sh
# Unpacks into the directory given the llvm-mingw release that cross-builds the extension for
# Windows x86_64 from macOS: Clang, LLD and the mingw-w64 headers and libraries against the UCRT.
# The archive is checked against its SHA-256 before it is unpacked. A toolchain already unpacked
# from this archive is kept.
set -eu

dir=$1
release=20261006
archive=llvm-mingw-$release-ucrt-macos-universal.tar.xz
sha256=a7a19b11048e89372637d26279cdb7c430ed3cf8c1e224310421312bf986ebf6

if [ "$(cat "$dir/.archive" 2>/dev/null)" = "$sha256 $archive" ]; then
  echo "kept the Windows toolchain in $dir, unpacked from $archive"
  exit 0
fi
rm -rf "$dir"
mkdir -p "$dir"
curl -sSfL -o "$dir/$archive" "https://github.com/mstorsjo/llvm-mingw/releases/download/$release/$archive"
echo "$sha256  $dir/$archive" | shasum -a 256 -c - >/dev/null
tar -xf "$dir/$archive" -C "$dir" --strip-components 1
rm -f "$dir/$archive"
echo "$sha256 $archive" > "$dir/.archive"
echo "unpacked the Windows toolchain into $dir from $archive, checked against its SHA-256"
