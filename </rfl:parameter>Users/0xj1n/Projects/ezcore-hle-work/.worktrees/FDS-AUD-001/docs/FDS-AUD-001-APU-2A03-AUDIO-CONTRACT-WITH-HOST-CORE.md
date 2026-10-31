# FDS-AUD-001 — APU (2A03) audio contract with host core

Status: **contract document — no code, no tests.** This is a *missing surface*
finding: the FDS HLE library in this repository contains no APU (2A03) audio
interface, no audio callbacks, no sample buffers, and no audio entry points.
This document records that fact and pins the only APU-related behaviour that
the allowed public sources (docs/SOURCES.md, S10) actually document, marking
everything else `UNKNOWN`. It does not invent an implementation.

- Scope: FDS-only HLE library (`0xJ1nn/ezcore-core-fds`). The 2A03 APU and
  its audio output are owned by the **host core**, not by this library.
- Sources (public documentation only):
  - S10: NESdev wiki, "FDS BIOS" — docs/SOURCES.md (entry points, register
    effects, LoadFiles contract; mentions the APU frame counter as a non-disk
    IRQ source).
  - S11: NESdev wiki, "FDS disk format" — docs/SOURCES.md (disk layout only).
  - S12: NESdev wiki, "FDS file format" — docs/SOURCES.md (file layout only).
- No BIOS, ROM, disassembly, emulator source, or leaked SDK material was
  consulted. `spec/nes/` is absent from this checkout; this document follows
  the sources mandated by docs/SOURCES.md.

--------------------------------------------------------------------------------

## 1. Missing surface finding

Verified against this checkout (grep of `include/`, `src/`, `tests/` for
`audio`, `APU`, `sample`, `sound`):

- No audio or APU symbols exist in `include/ezhle/ezhle.h`,
  `include/ezhle/ezhle_fds.h`, or `include/ezhle/ezhle_fds_image.h`.
- No audio or APU symbols exist in `src/` (the only APU references in the
  whole tree are the two APU-frame-counter mentions in
  `docs/FDS-LOADFILES.md`).
- No audio tests exist in `tests/`.
- `ezhle_desc_t` (include/ezhle/ezhle.h) exposes only `bus` and `sched`
  surfaces; there is no audio callback group, no sample-rate field, no buffer
  pointer, and no audio error code.
- `FDS_ENTRIES` (src/systems/fds/fds_hle.c) contains 36 documented BIOS
  entry points; none is an audio or APU routine.

Conclusion: **the audio contract between this HLE library and the host core is
not defined anywhere in this repository.** The host core owns all 2A03 APU
behaviour. This document is the finding, not a scaffold for a future
implementation.

--------------------------------------------------------------------------------

## 2. What the sources actually say about the APU

Source: S10 (docs/SOURCES.md), "Disk access routines" preamble.

The only APU-related statement the allowed sources make is a **caller
prerequisite** for the disk access routines (of which LoadFiles is one):

- The calling program must disable all non-disk IRQ sources before the call,
  explicitly including the **timer**, the **DMC**, and the **APU frame
  counter**.
- The calling program must disable NMIs before the call (e.g. via
  `JSR VINTWait`).

Everything else about the 2A03 APU — its registers, channels, timing, sample
generation, IRQ behaviour, and audio output — is **not** documented in
S10/S11/S12. Per docs/SOURCES.md and docs/CLEAN_ROOM.md, those facts are
`UNKNOWN` and must not be filled in from memory or another emulator.

--------------------------------------------------------------------------------

## 3. Host core responsibilities (documented only)

| Responsibility | Documented? | Source |
|---|---|---|
| Disable APU frame-counter IRQ around disk access routines | yes | S10 (docs/SOURCES.md) |
| Disable DMC IRQ around disk access routines | yes | S10 (docs/SOURCES.md) |
| Disable timer IRQ around disk access routines | yes | S10 (docs/SOURCES.md) |
| Disable NMIs around disk access routines | yes | S10 (docs/SOURCES.md) |
| Produce 2A03 audio samples | not documented for this HLE | UNKNOWN |
| Expose an audio callback/buffer to this library | no such surface exists | repo (section 1) |
| Translate HLE status into audio behaviour | no such surface exists | repo (section 1) |

The HLE library performs none of the above. It does not touch APU registers
(`$4000`–`$4017` or any other address), does not schedule audio, and does not
emit samples. Whether it *should* ever do so is undocumented and therefore
out of scope for this slice.

--------------------------------------------------------------------------------

## 4. Units

No audio units are defined by this repository or by S10/S11/S12. The
following are recorded as `UNKNOWN` and must not be hardcoded by a host core on
the strength of this document:

| Quantity | Value in this repo | Status | Source |
|---|---|---|---|
| CPU clock (2A03) frequency | not stated | UNKNOWN | S10/S11/S12 (silent) |
| Cycle unit used by the HLE | 2A03 CPU cycles (guest-visible timing only where a routine documents it) | partial | S10 (routine-level only) |
| Vblank frame rate | not stated | UNKNOWN | S10/S11/S12 (silent) |
| Audio sample rate | not stated | UNKNOWN | S10/S11/S12 (silent) |
| Sample bit depth | not stated | UNKNOWN | S10/S11/S12 (silent) |
| Channel count / mix | not stated | UNKNOWN | S10/S11/S12 (silent) |
| Buffer size / latency | not stated | UNKNOWN | S10/S11/S12 (silent) |
| Frame-counter period (4-step / 5-step) | not stated | UNKNOWN | S10/S11/S12 (silent) |
| DMC sample rate / DMA timing | not stated | UNKNOWN | S10/S11/S12 (silent) |

Notes:
- The HLE library's only cycle-level claim is routine-specific (e.g.
  `Delay131` = 131 CPU cycles, documented in FDS-COVERAGE.md and implemented
  in src/systems/fds/fds_hle.c). It makes no global clock claim.
- "Frame" in this repository means a vblank frame as used by the guest
  program; its frequency is not pinned here.

--------------------------------------------------------------------------------

## 5. Interface fields (host core -> HLE)

There is **no interface**. `ezhle_desc_t` has no audio field, and no accessor
exists. Enumerating the (empty) surface explicitly so a future addition is
visible as a change:

| Field | Type | Units | Direction | Status |
|---|---|---|---|---|
| (none) | — | — | — | missing surface (section 1) |

Any future audio surface added to `ezhle_desc_t` requires an
`EZHLE_ABI_VERSION` bump (include/ezhle/ezhle.h, "LAYOUT CONTRACT") and a
documented source citation in docs/SOURCES.md. Until then, a host core must
not pass audio data to this library and must not expect audio data from it.

--------------------------------------------------------------------------------

## 6. Inputs (audio-relevant, guest-visible)

| Input | Meaning | Units | Status | Source |
|---|---|---|---|---|
| APU frame-counter IRQ enable | caller-managed around disk routines | boolean | documented as "disable" only | S10 |
| DMC IRQ enable | caller-managed around disk routines | boolean | documented as "disable" only | S10 |
| Timer IRQ enable | caller-managed around disk routines | boolean | documented as "disable" only | S10 |
| NMI enable | caller-managed around disk routines | boolean | documented as "disable" only | S10 |
| APU registers `$4000`–`$4017` | not accessed by this HLE | bytes | not applicable (no access) | repo |
| Audio sample stream | not an input to this library | samples | not applicable | repo |

No other audio input is documented by S10/S11/S12. In particular, the exact
APU register map, the frame-counter step mode, and DMC behaviour are
**UNKNOWN** and must not be implemented from this document.

--------------------------------------------------------------------------------

## 7. Outputs (audio-relevant, guest-visible)

| Output | Meaning | Units | Status | Source |
|---|---|---|---|---|
| Audio samples | produced by host core APU, not by this HLE | samples | not applicable | repo |
| Audio IRQ to CPU | managed by host core APU | boolean/cycle | not documented here | UNKNOWN |
| HLE status on an audio request | no audio request exists | `ezhle_status` | not applicable | repo |

The HLE library produces no audio output of any kind.

--------------------------------------------------------------------------------

## 8. Error behaviour

- There is no audio entry point in `FDS_ENTRIES`, so no dispatch path returns
  an audio result. Consistent with FDS-COVERAGE.md: unimplemented/unknown
  entries return `EZHLE_ERR_UNIMPL` and set guest `A = $FF` (see
  docs/FDS-LOADFILES.md section 12 for the stub mapping convention).
- Host-core audio errors (underrun, buffer fault, etc.) are **not** translated
  by this library; no mapping exists. Mapping an internal `EZHLE_ERR_*` to an
  audio condition is `UNKNOWN` and must not be guessed.
- Bus faults on audio addresses: this library performs no audio bus accesses,
  so no fault path exists. If a future surface adds such accesses, the
  fault -> error mapping is `UNKNOWN` (see docs/FDS-LOADFILES.md section 11.10
  for the analogous disk case).
- `ezhle_last_error()` (include/ezhle/ezhle.h) stores a diagnostic string for
  the last dispatch; it carries no audio-specific code.

--------------------------------------------------------------------------------

## 9. Determinism / state save-load

Project rules (not wiki-derived), inherited from docs/FDS-LOADFILES.md
section 10:

- No wall clock in guest-visible paths. Any future audio timing must be
  cycle-count or sample-count based, never time-based.
- Save-state round-trips must reproduce identical audio output for identical
  inputs.
- The APU frame-counter IRQ disable around disk routines must be reproducible
  and must not depend on host timing.

--------------------------------------------------------------------------------

## 10. UNKNOWN (must not be guessed)

1. 2A03 CPU clock frequency (S10/S11/S12 silent).
2. Vblank frame rate (S10/S11/S12 silent).
3. Audio sample rate, bit depth, channel count, mixing (S10/S11/S12 silent).
4. APU frame-counter period and 4-step/5-step mode (S10/S11/S12 silent).
5. DMC sample rate, DMA timing, and IRQ behaviour (S10/S11/S12 silent).
6. APU register map and side effects (S10/S11/S12 silent).
7. Whether the HLE should ever enable/disable the APU frame counter itself
   (only the caller-disable prerequisite is documented).
8. Whether disk routines' timing depends on APU state (S10 silent).
9. Audio buffer size, latency, and underrun policy (no surface in repo).
10. Audio error codes and their mapping to `ezhle_status` (no surface in repo).
11. Whether any FDS BIOS entry point affects audio (none in FDS_ENTRIES is
    audio-related, but the wiki does not state this exhaustively; marked
    UNKNOWN rather than asserted).
12. Host core API shape for audio (no such API in this repo).

--------------------------------------------------------------------------------

## 11. HLE stub mapping obligations

There are no audio stubs today. If an audio surface is added later, it must
follow the convention in docs/FDS-LOADFILES.md section 12:

- Undocumented behaviour returns `EZHLE_ERR_UNIMPL` and sets guest `A = $FF`,
  with the call logged.
- No audio stub may report success (`A = $00`) for an undocumented operation.
- No test may be weakened to make an audio stub pass.
- Any new audio field in `ezhle_desc_t` requires an ABI version bump.

--------------------------------------------------------------------------------

## 12. Verification

Commands run for this document (documentation-only change):

- `make test` — unchanged expectation: tests/test_fds and tests/test_fds_image
  pass; no code touched.
- `make test-cxx` — unchanged.
- `sh scripts/check_clean.sh` — no ROM/BIOS/keys/binary/internal markers
  introduced (this file is prose only).

No implementation was added, so no new tests are required for this slice; the
missing surface itself is the finding recorded in section 1.
