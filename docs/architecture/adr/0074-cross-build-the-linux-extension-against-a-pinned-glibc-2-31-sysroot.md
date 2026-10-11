# ADR 0074: Cross-build the Linux extension against a pinned glibc 2.31 sysroot

> Status: accepted
> Date: 2026-10-11

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The owner decided on 2026-10-09 that the game targets macOS and Linux for now (issue #178). `make export` wrote the macOS app and the Windows executable; nothing built the extension as a Linux release library or exported the game for Linux. The Mac running `make export` is the only machine here: no Linux machine runs a build, and GitHub Actions are off.

A Linux library loads on every system whose glibc is at least the newest symbol version it binds, so the sysroot it links against sets the oldest system it runs on. Godot 4.7.2's `linux_release.x86_64` template binds glibc symbols up to `GLIBC_2.28` (`llvm-readelf -V`), so the extension's floor is the game's floor wherever it is above that.

Options weighed, on the development Mac (Apple Silicon) on 2026-10-11:

- **llvm-mingw 20261006's Clang and LLD against an Ubuntu 20.04 sysroot.** ADR 0033 already pins the archive, and its `clang++` and `ld.lld` target x86_64 Linux as well as Windows; its target defaults are set only for the mingw triples, so for `x86_64-linux-gnu` it uses the sysroot's libstdc++ and libgcc. The sysroot is glibc 2.31, GCC 10.5's libgcc and libstdc++ and the Linux 5.4 UAPI headers, seven packages from the same dated Ubuntu snapshot ADR 0016 pins, each checked by SHA-256; the shared runtimes of libgcc and libstdc++ are among them because CMake links a test executable against them when it configures. godot-cpp links libstdc++ and libgcc statically on Linux by default, so the library needs only glibc at run time: measured, `libc.so.6`, `libm.so.6` and `ld-linux-x86-64.so.2`, at symbol versions up to `GLIBC_2.14`.
- **The lint sysroot, Ubuntu 24.04.** Rejected: its glibc 2.39 would refuse Ubuntu 22.04 and Debian 12, and a libstdc++ archive built against it binds glibc symbols newer than any older sysroot holds, so it cannot be paired with an older glibc.
- **An older floor, glibc 2.28 as the template's, from Debian 10.** Rejected: it pairs with GCC 8's libstdc++, two major releases older than GCC 10's, to reach systems such as RHEL 8, while Ubuntu 18.04 and Debian 10, the last releases of each on a glibc older than 2.31, are past their standard support.
- **Zig as the cross compiler, targeting a glibc version by name.** Rejected unmeasured: a second Clang release beside llvm-mingw's to pin and move, and it links LLVM's libc++ where every other build of the extension on a GNU target uses libstdc++.
- **The platform's Apple Clang.** Rejected: it compiles for Linux, as `make lint` does, but ships no ELF linker.

## Decision

`make export` cross-builds the extension for Linux x86_64 as a release library with llvm-mingw 20261006's `clang++` and LLD, against the Ubuntu 20.04 sysroot that `linux-sysroot.sh` unpacks into `build/linux-sysroot/` from packages pinned by SHA-256, and exports it with Godot 4.7.2's Linux x86_64 release template. The glibc floor is 2.31.

## Consequences

- One Mac writes the macOS, Windows and Linux builds with one command, from the same `extension/CMakeLists.txt`. A cold build of the Linux library took 473 s on the development Mac on 2026-10-11, with no warnings.
- The exported Linux build runs where glibc is 2.31 or newer: Ubuntu 20.04, Debian 11 and later releases of each, among others. RHEL 8 and its rebuilds, on glibc 2.28, are refused.
- The first `make export` downloads about 11 MB of packages and unpacks 73 MB under `build/linux-sysroot/`.
- The snapshot date and hashes in `linux-sysroot.sh` are moved by hand, as `lint-sysroot.sh`'s are, since Dependabot does not watch them. Moving llvm-mingw's release moves the Linux compiler with it.
- The Linux build is not run by any check: nothing here runs Linux. Running it is the owner's share.

## Signs the decision was wrong

- The exported Linux build fails to load its library on a system with glibc 2.31 or newer, or the library needs a shared library beside glibc's.
- A change that builds with the platform's compiler fails to compile against GCC 10's libstdc++ often enough that the floor holds the code back.
- The snapshot service stops serving the pinned packages, and `make export` cannot run on a fresh Mac.
