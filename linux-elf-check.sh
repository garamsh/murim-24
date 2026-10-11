#!/bin/sh
# Checks that the Linux library given loads on glibc 2.31 and later (ADR 0074): an x86_64 ELF shared
# object needing no library beside glibc's, libgcc_s and libstdc++, and no glibc symbol version newer
# than 2.31. It first builds, with the toolchain in the directory given, a control library needing a
# GLIBC_2.39 symbol from a stand-in libc, and fails unless the check refuses it, so a check that
# passes everything cannot pass unnoticed.
# Usage: linux-elf-check.sh <toolchain bin directory> <work directory> <library>
set -eu

bin=$1
work=$2
library=$3
floor=2.31
allowed="libc.so.6 libm.so.6 ld-linux-x86-64.so.2 libgcc_s.so.1 libstdc++.so.6"

# Prints why `$1` breaks the floor, one line a fault, nothing when it keeps it.
faults() {
  "$bin/llvm-readelf" -h "$1" | grep -q 'Class:[[:space:]]*ELF64' || echo "not ELF64"
  "$bin/llvm-readelf" -h "$1" | grep -q 'Machine:.*X86-64' || echo "not x86_64"
  "$bin/llvm-readelf" -h "$1" | grep -q 'Type:[[:space:]]*DYN' || echo "not a shared object"
  for needed in $("$bin/llvm-readelf" -d "$1" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'); do
    case " $allowed " in
      *" $needed "*) ;;
      *) echo "needs $needed" ;;
    esac
  done
  for version in $("$bin/llvm-readelf" -V "$1" | grep -o 'GLIBC_[0-9][0-9.]*' | sort -u); do
    newest=$(printf '%s\n%s\n' "${version#GLIBC_}" "$floor" | sort -V | tail -n 1)
    [ "$newest" = "$floor" ] || echo "needs $version, newer than GLIBC_$floor"
  done
}

rm -rf "$work"
mkdir -p "$work"
printf 'int newer(void) { return 1; }\n' > "$work/libc.c"
printf 'GLIBC_2.39 { global: newer; local: *; };\n' > "$work/libc.map"
printf 'int newer(void);\nint use(void) { return newer(); }\n' > "$work/control.c"
cross="$bin/clang --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -shared -fPIC"
$cross -Wl,--version-script="$work/libc.map" -Wl,-soname,libc.so.6 "$work/libc.c" -o "$work/libc.so.6"
$cross "$work/control.c" "$work/libc.so.6" -o "$work/libcontrol.so"
if [ -z "$(faults "$work/libcontrol.so")" ]; then
  echo "the check accepted $work/libcontrol.so, which needs GLIBC_2.39" >&2
  exit 1
fi
echo "refused the control library, which needs GLIBC_2.39: $(faults "$work/libcontrol.so" | tr '\n' ' ')"

found=$(faults "$library")
if [ -n "$found" ]; then
  echo "$library breaks the glibc $floor floor: $(echo "$found" | tr '\n' ' ')" >&2
  exit 1
fi
echo "$library is an x86_64 ELF shared object needing only $("$bin/llvm-readelf" -d "$library" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p' | tr '\n' ' ')and glibc symbols up to $("$bin/llvm-readelf" -V "$library" | grep -o 'GLIBC_[0-9][0-9.]*' | sort -uV | tail -n 1), within GLIBC_$floor"
