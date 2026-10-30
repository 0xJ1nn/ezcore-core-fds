# Clean-room policy (FDS HLE)

This repository is a clean-room implementation of documented Famicom Disk
System behavior. It is not a firmware dump, a BIOS replacement built from a
dump, or a disassembly of Nintendo code.

## Hard rules

1. **No firmware, BIOS, ROM, key, or leaked SDK material**, in any form:
   source, binary, test fixture, screenshot, patch, or commit history.
2. **Public documentation only.** Every implemented service cites a URL in
   `docs/SOURCES.md`. The current source is the NESdev wiki.
3. **UNKNOWN stays UNKNOWN.** If a behavior is not documented, the HLE returns
   `EZHLE_ERR_UNIMPL`, sets `A=$FF`, and logs the call. It does not guess.
4. **No source copying from other emulators.** GPL/non-commercial emulator
   source is a black-box oracle only; it is never copied into this BSD-2
   repository. No oracle source is read into this tree.
5. **Synthetic fixtures only.** Tests build disk images byte by byte in C.
   They do not embed a game, BIOS, or a real disk image.
6. **No disassembly of prohibited material.** Nothing in this repository was
   produced by disassembling a Nintendo BIOS or game.
7. **Review before merge.** A change needs a passing `make test`, `make asan`,
   `make test-cxx`, and `scripts/check_clean.sh`, plus an independent review.

## What the tests prove

The tests prove parser bounds, file-table handling, service dispatch, register
effects, error paths and memory safety. They do **not** prove that a game boots
or plays. No compatibility claim is made until a host core runs a real game and
the result is recorded as evidence.

## Reporting a provenance problem

Open an issue with the file and line, do not copy the suspect content into the
issue, and stop shipping the affected artifact until the maintainer clears it.
