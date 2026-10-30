# Host NES CPU core (Ricoh 2A03) timing contract

Status: **contract document — no code, no tests, no implementation.** This pins
the interface a host NES CPU core (Ricoh 2A03) must provide to this HLE
library, derived only from the public sources listed in `docs/SOURCES.md`
(S10, S11, S12) plus the in-tree ABI headers. Anything the sources are silent
about is marked `UNKNOWN` and must not be guessed; the HLE continues to return
`EZHLE_ERR_UNIMPL` for undocumented behavior.

No firmware, BIOS, ROM, key, leaked SDK, or disassembly was consulted. No host
core exists in this repository; the missing integration surface is itself the
finding recorded here.

--------------------------------------------------------------------------------

## 1. Scope and parties

- **HLE library** (this repo): FDS BIOS HLE, 5 of 36 entry points implemented
  (see `docs/FDS-COVERAGE.md`). It never executes 6502 instructions, never
  owns a clock, and never blocks a host thread.
- **Host core** (out of tree, not in this repo): emulates the Ricoh 2A03 CPU
  (and, for FDS, the RP2C33 PPU, APU and disk drive). It owns the memory map,
  the clock, interrupts, frames and audio.
- **Contract**: how the host core hands CPU state to the HLE, how the HLE
  reads/writes guest memory, and what timing guarantees the HLE assumes or
  requires from the host. The HLE makes no timing guarantees of its own.

Sources for FDS behavior: S10 (NESdev wiki, "FDS BIOS"), S11 (NESdev wiki,
"FDS disk format"), S12 (NESdev wiki, "FDS file format"). ABI shapes:
`include/ezhle/ezhle.h`, `include/ezhle/ezhle_fds.h`.

--------------------------------------------------------------------------------

## 2. CPU register handoff

The host core passes a 6502 register file to `ezhle_dispatch()` /
`ezhle_fds_call()` in `ezhle_regs_t` (`include/ezhle/ezhle.h`).

| Field | Type | Units | Direction | Meaning | Source |
|---|---|---|---|---|---|
| `regs.r[EZHLE_FDS_REG_A]` (index 0) | `uint32_t` (low 8 bits used) | 8-bit value | in/out | Accumulator A | S10 (register effects per entry) |
| `regs.r[EZHLE_FDS_REG_X]` (index 1) | `uint32_t` (low 8 bits used) | 8-bit value | in/out | X index | S10 |
| `regs.r[EZHLE_FDS_REG_Y]` (index 2) | `uint32_t` (low 8 bits used) | 8-bit value | in/out | Y index | S10 |
| `regs.r[EZHLE_FDS_REG_P]` (index 3) | `uint32_t` (low 8 bits used) | 8-bit value | in/out | Processor status P | S10 |
| `regs.r[EZHLE_FDS_REG_SP]` (index 4) | `uint32_t` (low 8 bits used) | 8-bit value | in/out | Stack pointer SP | S10 |
| `regs.pc` | `uint32_t` (low 16 bits used) | 16-bit address | in/out | Program counter; set to entry address before dispatch | S10 |
| `regs.r[5..31]` | `uint32_t` | — | unused | Not defined for 6502; host core must not rely on them | ABI (`ezhle.h`) |
| `regs.hi`, `regs.lo` | `uint32_t` | — | unused | MIPS-style; not used by FDS | ABI (`ezhle.h`) |

Flag bits (`include/ezhle/ezhle_fds.h`): `EZ_HLE_FDS_FLAG_C/Z/I/D/B/V/N`.
The host core is responsible for translating between its internal 6502 P and
this byte. Which flags each entry reads/writes beyond those documented per
entry is UNKNOWN (S10 is silent for most entries).

--------------------------------------------------------------------------------

## 3. Memory / bus interface

The HLE never dereferences a guest pointer. All access goes through
`ezhle_bus_t` (`include/ezhle/ezhle.h`).

| Callback | Signature | Units | Error behaviour | Source |
|---|---|---|---|---|
| `read32` | `int (*)(ctx, addr, *out)` | `addr`: 32-bit byte address; `*out`: 32-bit word | non-zero => `EZHLE_ERR_FAULT` | ABI |
| `write32` | `int (*)(ctx, addr, value)` | same | non-zero => `EZHLE_ERR_FAULT` | ABI |
| `read_bytes` | `int (*)(ctx, addr, dst, len)` | `len`: bytes | non-zero => `EZHLE_ERR_FAULT`; if NULL, HLE falls back to `read32` | ABI |
| `log` | `void (*)(ctx, level, msg)` | level int | optional (NULL allowed) | ABI |
| `trace` | `void (*)(ctx, nr, args, argc, ret)` | — | optional (NULL allowed) | ABI |
| `ctx` | `void *` | opaque | host-owned | ABI |

Address width for FDS is 16 bits (`0x0000-0xFFFF`); the host core's memory map
must cover at least that. Whether accesses above `0xFFFF` are defined is
UNKNOWN (S10/S11 silent; FDS uses 16-bit addressing).

Error mapping: a non-zero bus callback result becomes `EZHLE_ERR_FAULT`
internally. Mapping that to a guest-visible BIOS error code is UNKNOWN (see
`docs/FDS-LOADFILES.md` section 11.10 and `docs/CLEAN_ROOM.md`).

--------------------------------------------------------------------------------

## 4. Timing units and accounting

This is the core of the contract. The HLE library itself has **no clock** and
performs **no timing**. It reports no cycle counts, frame counts, or sample
counts. Any timing effect of a BIOS routine is either (a) documented as a
no-op in the HLE with the burden on the host core, or (b) UNKNOWN.

### 4.1 Documented timing facts (from S10)

| Fact | Value | Unit | HLE behaviour | Source |
|---|---|---|---|---|
| `Delay131` (`$E149`) delay | 131 | CPU cycles | Implemented as a no-op (`fds_delay131` returns 0 immediately). The host core receives control back with no elapsed-cycle accounting from the HLE. | S10 (entry name "Delay131"); `src/systems/fds/fds_hle.c` |
| CPU clock of Ricoh 2A03 | UNKNOWN | cycles/second | Not documented in S10/S11/S12. Commonly cited as ~1.79 MHz NTSC, but that value is not in the allowed sources, so it is UNKNOWN here. | S10 (silent) |
| Frames per second / vblank period | UNKNOWN | frames/second, cycles/frame | S10 mentions VINTWait (vblank sync) but gives no period. | S10 (silent) |
| PPU cycles per CPU cycle | UNKNOWN | ratio | Not in S10/S11/S12. | S10 (silent) |
| Scanlines per frame | UNKNOWN | scanlines | Not in S10/S11/S12. | S10 (silent) |
| Audio sample rate | UNKNOWN | samples/second | Not in S10/S11/S12. | S10 (silent) |
| APU frame counter rate | UNKNOWN | Hz | Not in S10/S11/S12. | S10 (silent) |
| Disk transfer rate | UNKNOWN | bytes/second or cycles/byte | S10/S11 describe disk format, not transfer timing. | S10, S11 (silent) |
| Disk motor spin-up / seek time | UNKNOWN | ms or cycles | Not documented. | S10 (silent) |

### 4.2 Consequence for Delay131

Because `Delay131` is a documented 131-cycle delay (S10, entry name) but the
HLE implements it as a no-op, a host core that calls `ezhle_fds_call(core,
regs, 0xE149)` will **not** observe 131 elapsed CPU cycles from the HLE. The
host core must decide one of:

1. Compensate by advancing its own cycle counter by 131 when this entry is
   dispatched; or
2. Accept the timing discrepancy (a correctness/performance risk).

Which is correct is **UNKNOWN** — S10 documents the routine's existence and
name but not whether an HLE may elide the delay. The HLE's current no-op is
recorded in `docs/FDS-COVERAGE.md` as `implemented` with the note
"131-cycle delay (no-op in HLE)". No cycle-return field exists in the ABI, so
the HLE cannot communicate a delta even if it wanted to. Adding such a field
would be a new ABI surface and is out of scope for this document.

### 4.3 VINTWait / disk timing

`VINTWait` (`$E1B2`) is documented as a vblank synchronization gate but is
currently `unimpl` (returns `EZHLE_ERR_UNIMPL`). The HLE therefore performs no
vblank wait. The host core must provide vblank synchronization itself. The
exact vblank period, NMI timing, and whether `VINTWait` polls a hardware flag
or blocks on NMI are **UNKNOWN** (S10 describes the entry but not its timing
contract precisely enough to pin a period).

--------------------------------------------------------------------------------

## 5. Interrupt contract

S10 states, for the disk access routines (including `LoadFiles`):

- The calling program must disable all non-disk IRQ sources (timer, DMC, APU
  frame counter) before the call.
- The calling program must disable NMIs before the call (e.g. via `VINTWait`).
- `[$0101]` (IRQ vector-control byte) is preserved on entry and restored on
  exit.

| Item | Unit | HLE behaviour | Source |
|---|---|---|---|
| IRQ vector-control byte at `$0101` | byte | Preserved/restored by the routine (documented for disk access routines). The HLE does not currently implement LoadFiles, so it does not touch `$0101`. | S10 |
| Non-disk IRQs during disk I/O | — | Caller's responsibility; HLE does not mask them. | S10 |
| NMIs during disk I/O | — | Caller's responsibility; HLE does not mask them. | S10 |
| Disk IRQ delivery (RP2C33 -> CPU IRQ) | — | UNKNOWN. The HLE has no interrupt controller and no IRQ line. Whether and when the host core must assert a disk IRQ is not documented in S10/S11/S12. | S10 (silent) |
| NMI delivery from PPU vblank | — | UNKNOWN timing; host core owns it. | S10 (silent) |

--------------------------------------------------------------------------------

## 6. Frame / rendering timing

The FDS BIOS VRAM helpers (`VRAMStructWrite`, `WriteVRAMBuffer`,
`ReadVRAMBuffer`, etc.) are `unimpl`. The HLE performs no VRAM transfers and
no frame scheduling. The host core (PPU) owns frame timing, scanline timing,
vblank NMI and VRAM access windows. All of these are **UNKNOWN** with respect
to S10/S11/S12, which describe data formats but not timing.

| Item | Status | Source |
|---|---|---|
| Scanline count / vblank start | UNKNOWN | S10 (silent) |
| VRAM access timing (during vblank vs. rendering) | UNKNOWN | S10 (silent) |
| Frame rate (NTSC/PAL) | UNKNOWN | S10 (silent) |

--------------------------------------------------------------------------------

## 7. Audio timing

The HLE contains no audio code. The host core owns APU timing, sample
generation, and the FDS expansion audio (the FDS has its own wavetable channel
in the RP2C33). None of the allowed sources document audio timing, sample
rates, or the FDS expansion audio register timing. All audio timing is
**UNKNOWN**.

| Item | Status | Source |
|---|---|---|
| Sample rate | UNKNOWN | S10/S11/S12 (silent) |
| APU frame counter / envelope timing | UNKNOWN | S10/S11/S12 (silent) |
| FDS expansion audio (wavetable) timing | UNKNOWN | S10/S11/S12 (silent) |
| Audio buffer / underrun policy | UNKNOWN | S10/S11/S12 (silent) |

--------------------------------------------------------------------------------

## 8. Disk drive timing

The HLE attaches an in-memory `.fds` image (see `ezhle_fds_attach_disk` and
`docs/FDS-LOADFILES.md`). It has no drive model: no motor, no head, no side
ejection/insertion, no CRC hardware. The host core must supply any drive
timing. Per the sources:

| Item | Documented? | Notes | Source |
|---|---|---|---|
| Disk info block layout | Yes | `$0000` block 1 etc. | S11 |
| File header / data blocks | Yes | offsets and fields | S11 |
| Boot read file code at disk info offset `$19` | Yes | used by LoadFiles boot-files mode | S11, S10 |
| Side count / multi-side ordering | Yes | S12 | S12 |
| Drive ready / disk-set bit (`$4032.0`) | Mentioned | Used in eject/insert polling example | S10 |
| Disk IRQ timing | No | UNKNOWN | S10 (silent) |
| Transfer rate (bytes/s or cycles/byte) | No | UNKNOWN | S10/S11 (silent) |
| Motor spin-up / seek | No | UNKNOWN | S10 (silent) |
| CRC generation (RP2C33) | Mentioned | CRCs omitted from `.FDS`; `$27` error code exists | S10, S11 |
| Multi-side auto-advance by LoadFiles | No | UNKNOWN; example shows game-side eject/insert loop | S10, `docs/FDS-LOADFILES.md` 9.4 |

--------------------------------------------------------------------------------

## 9. Scheduler interface (optional)

`ezhle_desc_t` carries an optional `ezhle_sched_t` (packed aliases or flat
callbacks) documented in `include/ezhle/ezhle.h`. The HLE never blocks a host
thread; it reports yield intent and the host decides re-dispatch. All scheduler
fields are optional (NULL = no scheduler hooked).

| Field | Meaning | Source |
|---|---|---|
| `yield` / aliases | report a blocked guest thread | ABI |
| `make_ready` / aliases | mark a thread ready | ABI |
| `ctx` / aliases | opaque scheduler context | ABI |
| Yield reasons (`EZHLE_SCHED_*`) | informational | ABI |

Timing of re-dispatch, thread quantum, and preemption are **not** documented
in S10/S11/S12 and are host-core policy (UNKNOWN from the FDS sources).

--------------------------------------------------------------------------------

## 10. Error behaviour

Internal HLE status codes (`include/ezhle/ezhle.h`):

| Code | Value | Meaning |
|---|---|---|
| `EZHLE_OK` | 0 | handled |
| `EZHLE_ERR_ARG` | -1 | bad argument |
| `EZHLE_ERR_NOMEM` | -2 | allocation failure |
| `EZHLE_ERR_UNIMPL` | -3 | no handler / undocumented |
| `EZHLE_ERR_FAULT` | -4 | bus fault |
| `EZHLE_ERR_STATE` | -5 | bad state |

Guest-visible `A` translation (for entries that document it) follows
`docs/FDS-LOADFILES.md` section 12. For undocumented entries, the stub sets
`A = $FF` and logs (see `docs/FDS-COVERAGE.md`). The host core must not
interpret `EZHLE_ERR_*` as guest-visible values; they are internal.

--------------------------------------------------------------------------------

## 11. Missing surface (finding)

This repository contains **no** host NES CPU core, no timing API, no cycle
counter, no frame scheduler, no audio path, and no disk-drive model. The HLE
library is a pure function of (core, regs, bus) and performs no timing. Any
integration with a Ricoh 2A03 host core is therefore out of scope of this
repository and must be provided by the host. Concretely, the following are
absent and would be needed for real integration (all currently UNKNOWN or
unimplemented):

1. A host-side cycle accounting hook so `Delay131`'s 131 cycles are accounted.
2. A vblank/frame timing source for `VINTWait`.
3. Disk IRQ assertion timing.
4. A drive model for side ejection/insertion and transfer timing.
5. Audio/sample timing for the FDS expansion audio.
6. A guest-visible error mapping from `EZHLE_ERR_FAULT` to a BIOS error code.

None of the above may be invented here; each is recorded as UNKNOWN until a
public source (S10/S11/S12 or a future source added to `docs/SOURCES.md`)
documents it.

--------------------------------------------------------------------------------

## 12. UNKNOWN list (explicit)

1. Ricoh 2A03 CPU clock frequency (cycles/second). (S10 silent)
2. Frames per second and vblank period. (S10 silent)
3. PPU cycles per CPU cycle. (S10 silent)
4. Scanlines per frame. (S10 silent)
5. Audio sample rate. (S10/S11/S12 silent)
6. APU frame counter rate. (S10/S11/S12 silent)
7. Disk transfer rate. (S10/S11 silent)
8. Disk motor spin-up / seek time. (S10 silent)
9. Disk IRQ delivery timing. (S10 silent)
10. Whether `Delay131`'s 131-cycle delay must be compensated by the host core. (S10 silent on HLE elision)
11. Exact `VINTWait` timing and polling target. (S10 silent)
12. VRAM access timing. (S10 silent)
13. Frame rate NTSC/PAL. (S10 silent)
14. FDS expansion audio timing. (S10/S11/S12 silent)
15. Audio buffer/underrun policy. (S10/S11/S12 silent)
16. Multi-side auto-advance by LoadFiles. (S10 silent; see `docs/FDS-LOADFILES.md` 9.4)
17. Guest-visible error code for a bus fault. (S10 silent; see `docs/FDS-LOADFILES.md` 11.10)
18. Re-dispatch timing / thread quantum. (S10/S11/S12 silent)
19. Any timing not explicitly listed in section 4.1.

--------------------------------------------------------------------------------

## Sources

- S10 — NESdev wiki, "FDS BIOS": https://www.nesdev.org/wiki/FDS_BIOS
  (entry names including Delay131; disk access routine prerequisites; Disk ID;
  error list; VINTWait).
- S11 — NESdev wiki, "FDS disk format": https://www.nesdev.org/wiki/FDS_disk_format
  (disk info block; file header; CRCs omitted from `.FDS`).
- S12 — NESdev wiki, "FDS file format": https://www.nesdev.org/wiki/FDS_file_format
  (multi-side ordering).
- ABI shapes: `include/ezhle/ezhle.h`, `include/ezhle/ezhle_fds.h`
  (not behavioral sources; cited for field names/types only).
- `docs/FDS-COVERAGE.md`, `docs/FDS-LOADFILES.md`, `docs/CLEAN_ROOM.md`
  (in-repo status and error conventions).
