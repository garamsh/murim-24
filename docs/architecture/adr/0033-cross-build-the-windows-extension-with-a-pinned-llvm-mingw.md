# ADR 0033: Cross-build the Windows extension with a pinned llvm-mingw

> Status: accepted
> Date: 2026-10-08

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

`make export` writes runnable PC builds for macOS and Windows x86_64 (issue #131), and each loads the extension built for it. The Mac building them has no compiler that targets Windows: the platform's default compiler, the one every other build uses, targets Apple platforms only.

Options weighed, measured on the development Mac (Apple Silicon) on 2026-10-08:

- **llvm-mingw 20261006, `llvm-mingw-20261006-ucrt-macos-universal.tar.xz`, from github.com/mstorsjo/llvm-mingw.** Clang 23.1.3, LLD and the mingw-w64 headers and libraries against the UCRT, in one 124 MB archive that unpacks under `build/` with no system install. It cross-configured `extension/` unchanged with `CMAKE_SYSTEM_NAME=Windows` and built the `template_release` library from a cold tree in 97 s with no warnings under `-Werror`. The DLL is 1.2 MB and imports only `KERNEL32.dll` and the `api-ms-win-crt-*` UCRT sets, which Windows 10 and later carry, so no runtime library ships beside it.
- **Homebrew mingw-w64 14.0.0 (GCC).** Rejected unmeasured: a system install that Homebrew moves on its own schedule, which no pin in the repository fixes.
- **A Windows job on the CI runner.** Rejected: `make export` could not write the DLL on the machine running it, so the export would depend on downloading a pipeline artifact.

## Decision

`make export` cross-builds the extension for Windows x86_64 with llvm-mingw 20261006, which `mingw-toolchain.sh` fetches into `build/mingw/` and checks against its SHA-256 before unpacking. Every other build keeps the platform's default compiler.

## Consequences

- One Mac writes both PC builds with one command, and the Windows library is built from the same `extension/CMakeLists.txt` as every other.
- The first `make export` downloads 124 MB and unpacks it under `build/mingw/`.
- The archive is built for macOS, so `make export` runs on a Mac; it also builds the macOS universal library with the Mac's SDK.
- The release and hash in `mingw-toolchain.sh` are moved by hand, since Dependabot does not watch them.
- The Windows build is not run by any check: nothing here runs Windows.

## Signs the decision was wrong

- The exported Windows build fails to load its DLL on a Windows 10 or later machine, or needs a runtime library beside it.
- A change that builds with the platform's compiler fails to cross-build with llvm-mingw, or the reverse, often enough that the two disagree as a matter of course.
- The release asset is withdrawn, and `make export` cannot run on a fresh Mac.
