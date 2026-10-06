# ADR 0016: Lint against one pinned Linux sysroot on every host

> Status: accepted
> Date: 2026-10-06

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

ADR 0003 has every host install the same clang-tidy 22.1.8, and names as a sign of a wrong decision that `make lint` passes locally and fails in CI on the same commit. That happened on PR #49 at `ca8367d` (issue #50). CI on Linux reported `cppcoreguidelines-pro-type-member-init` on `Structure` in `core/world.hpp`; the macOS run did not.

The cause is the standard library each host parses against, not the tool:

- The check examines a struct with no constructor declared in the translation unit's AST.
- Clang declares a struct's implicit constructors only when the code it parses needs them.
- In `core/world.cpp` at `ca8367d`, the macOS SDK's libc++ needed them, so `Structure` had three implicit constructors declared and the check skipped it. Against Ubuntu 24.04's libstdc++ 13, `Structure` had none and the check reported it. Both runs used Clang 22.1.8's `-ast-dump` and the same source.
- Each host's lint read its compile commands from its own build, so it parsed against its own system headers: macOS through `CMAKE_OSX_SYSROOT`, Linux through GCC 13's.

Options weighed:

- **One pinned sysroot for lint on every host.** Ubuntu 24.04's x86_64 libstdc++ 13, glibc and Linux UAPI headers, fetched from a dated snapshot of the Ubuntu archive and checked by SHA-256. Lint configures CMake for `x86_64-linux-gnu` against them, compiling only, so the platform's `clang++` serves on either host. Every host then parses the same text with the same tool.
- **Lint in CI on macOS.** Rejected. The runner's SDK moves with its Xcode, not with any pin. The Linux job would still lint against GCC's headers, so a finding could appear in one CI job and not the other.
- **libc++ on Linux.** Rejected. LLVM's libc++ and the macOS SDK's are different releases, so the parser inputs would still differ.
- **Disabling the check.** Rejected. It catches uninitialized members in constructors, and the same divergence can come from any check that reads what the standard library instantiates.

## Decision

`make lint` parses the core and the extension on every host as an Ubuntu 24.04 x86_64 build, against headers unpacked by `lint-sysroot.sh` from Ubuntu snapshot packages pinned by SHA-256. Builds and tests keep each host's own compiler and standard library.

## Consequences

- Local and CI lint read the same headers, flags and tool, so they report the same findings on the same commit. On `ca8367d` both report the finding CI reported.
- The first `make lint` on a host downloads about 9 MB of packages and unpacks 60 MB under `build/lint/`. A host needs `clang++`, `curl`, `ar`, `tar` with zstd, and `shasum`.
- Lint no longer sees a finding that only the macOS SDK's libc++, or a newer libstdc++, would expose. The build still compiles on every host with warnings as errors.
- The snapshot date and hashes in `lint-sysroot.sh` are moved by hand, together with the GCC release the CI runner builds with, since Dependabot does not watch them.

## Signs the decision was wrong

- A lint finding differs between hosts again, on the same commit.
- A change that builds and lints cleanly fails to compile on the macOS SDK's libc++ in a way a lint against it would have caught.
- The snapshot service stops serving the pinned packages, and lint cannot run on a fresh host.
