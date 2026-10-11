#!/bin/sh
# Unpacks into the directory given the Ubuntu 20.04 x86_64 sysroot the extension is cross-built and
# linked against for Linux: glibc 2.31, GCC 10's libgcc and libstdc++, and the Linux UAPI headers, so
# the library needs no glibc newer than 2.31. Each package comes from a dated snapshot of the Ubuntu
# archive and is checked against its SHA-256 before it is unpacked; nothing in it is run. A sysroot
# already unpacked from these packages is kept.
set -eu

dir=$1
snapshot=https://snapshot.ubuntu.com/ubuntu/20261006T000000Z/pool
packages="
1b2281aac4935dfea1f89dfc19e445fdbb45303202af60679ccb8bf035f081a0 main/g/glibc/libc6_2.31-0ubuntu9.18_amd64.deb
11029900f2315b93ce487792ae1bf15f0927e5e50880c091e6a440b14d4e164a main/g/glibc/libc6-dev_2.31-0ubuntu9.18_amd64.deb
4aa7b9c9f3225df65a750ae0ff5c890fc6108c82a609b3c5abd45d211838bf3c main/g/gcc-10/libgcc-s1_10.5.0-1ubuntu1~20.04_amd64.deb
ebe5e967fca32ea5811946180f7822ee749857861eadaa29514e65857ce3b897 main/g/gcc-10/libgcc-10-dev_10.5.0-1ubuntu1~20.04_amd64.deb
7f9222342d3551d063bf651319ec397c39278eeeb9ab5950ae0e8c28ef0af431 main/g/gcc-10/libstdc++6_10.5.0-1ubuntu1~20.04_amd64.deb
106ae470c9b34785f1b1ff06fa633d0899c8dd415979055aad8899b949ca2491 universe/g/gcc-10/libstdc++-10-dev_10.5.0-1ubuntu1~20.04_amd64.deb
70c9addd36aabd2d031d5ea0fc444f8b193027aef79ff3d8275d25800aed8ede main/l/linux/linux-libc-dev_5.4.0-216.236_amd64.deb
"

if [ "$(cat "$dir/.packages" 2>/dev/null)" = "$(echo "$packages")" ]; then
  echo "kept the Linux sysroot in $dir, unpacked from the same $(echo "$packages" | grep -c deb) packages"
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
echo "unpacked the Linux sysroot into $dir from $(echo "$packages" | grep -c deb) packages, each checked against its SHA-256"
