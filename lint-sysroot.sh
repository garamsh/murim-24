#!/bin/sh
# Unpacks into the directory given the Ubuntu 24.04 x86_64 headers clang-tidy parses the C++ against
# on every host, so local and CI lint read the same standard library (ADR 0016): GCC 13's
# libstdc++, glibc and the Linux UAPI headers, and GCC's own directory, by which clang finds them.
# Each package comes from a dated snapshot of the Ubuntu archive and is checked against its SHA-256
# before it is unpacked; nothing in it is run. A sysroot already unpacked from these packages is kept.
set -eu

dir=$1
snapshot=https://snapshot.ubuntu.com/ubuntu/20261006T000000Z/pool/main
packages="
ee5633e863e19c3381ed97842ce35ed32ede96a3d1ae4e94c051d3036fe21347 g/gcc-13/libstdc++-13-dev_13.3.0-6ubuntu2~24.04.1_amd64.deb
cd689db2691edaa10f37329307292796bb599e722e0505c79e14caaa1fe9a93a g/gcc-13/libgcc-13-dev_13.3.0-6ubuntu2~24.04.1_amd64.deb
e13d5fcc1b2a86f75bca8e0026a8e39f24fe97ca86e92be79991f8697eb1306f g/glibc/libc6-dev_2.39-0ubuntu8.9_amd64.deb
937db1a88a4fa2ea97fd4eab89f2cd9d077290f6a26bcd27b1a6d24fa3d706b6 l/linux/linux-libc-dev_6.8.0-142.142_amd64.deb
"

if [ "$(cat "$dir/.packages" 2>/dev/null)" = "$(echo "$packages")" ]; then
  echo "kept the lint sysroot in $dir, unpacked from the same $(echo "$packages" | grep -c deb) packages"
  exit 0
fi
rm -rf "$dir"
mkdir -p "$dir/.unpack"
echo "$packages" | while read -r sha256 path; do
  [ -n "$path" ] || continue
  deb="$dir/.unpack/package.deb"
  curl -sSfL -o "$deb" "$snapshot/$path"
  echo "$sha256  $deb" | shasum -a 256 -c - >/dev/null
  (cd "$dir/.unpack" && ar x package.deb && tar -xf data.tar.* -C .. && rm -f ./*)
done
rm -rf "$dir/.unpack"
echo "$packages" > "$dir/.packages"
echo "unpacked the lint sysroot into $dir from $(echo "$packages" | grep -c deb) packages, each checked against its SHA-256"
