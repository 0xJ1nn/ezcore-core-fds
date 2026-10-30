# Language and ABI

## Decision

The HLE core is **C11** with a **stable C ABI**. Public headers are wrapped in
`extern "C"` so C++17 hosts can include and link them directly. C++ is allowed
for frontends, host adapters and tooling; it is not required in the core.

## Why not rewrite the core in C++

- **Speed is not a language feature here.** The hot paths are table dispatch,
  byte parsing and memory copies. C11 with `-O2` compiles those to the same
  machine code a C++ compiler would produce. C++ virtual dispatch, exceptions
  and RTTI would add cost unless disabled, at which point the code is C-style
  C++ with extra build complexity.
- **Portability is the product requirement.** A plain C ABI links from Dart
  FFI (Flutter), JNI/NDK (Android), Objective-C/Swift (iOS/macOS), libretro and
  C hosts without a C++ runtime or standard-library version match. C++ ABIs
  differ across NDK, Apple platforms and consoles.
- **Binary size and startup.** No `libc++`/`libstdc++` dependency, no static
  initializers, no exception tables. That matters on embedded and console
  targets.
- **The existing code is already tested.** Rewriting working, ASan-clean C in
  C++ would add risk without a measured performance gain.

## What we do instead

- Keep the core `-std=c11 -Wall -Wextra -Werror -pedantic`.
- Keep every public header C++-safe (`extern "C"`).
- Run `make test-cxx` in CI: a C++17 translation unit includes all public
  headers and links `libezhle.a`. This is a real ABI test, not a claim.
- Allow C++17 in a host adapter or platform pack only if it exposes a C ABI and
  passes the same smoke test.
- Optimize measured hot paths with data layout, branch reduction and profiling,
  not by changing language.

## Revisit trigger

Revisit only with a measured case where C11 cannot meet the target and a C++
implementation wins on the same hardware. Record the benchmark and the ABI
cost in an ADR before changing the core.
