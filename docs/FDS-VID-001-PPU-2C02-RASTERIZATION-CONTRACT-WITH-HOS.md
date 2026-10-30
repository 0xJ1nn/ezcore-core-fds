# FDS-VID-001 — PPU (2C02) rasterization contract with host core

Status: **contract document — no code, no tests.** This pins the boundary
between the FDS HLE library and the host core's PPU (Ricoh 2C02)
rasterization, from public documentation only (NESdev wiki, FDS BIOS page).
Anything the allowed sources do not document is marked `UNKNOWN` and must not
be guessed; until a behaviour is documented, the HLE reports
`EZHLE_ERR_UNIMPL` for the BIOS entry points that touch it. No PPU
rasterization implementation is added by this document.

- Surface: PPU (2C02) rasterization and the FDS BIOS PPU-helper entry points.
- Host core role: owns the 2C02, its registers, VRAM, OAM, palette and the
  produced framebuffer. The HLE library never rasterizes.
- HLE library role: dispatches FDS BIOS entry points that read/write guest
  memory and PPU state through the bus callbacks in `ezhle_bus_t`
  (`include/ezhle/ezhle.h`); it does not own a PPU.
- Sources (public documentation only):
  - S10: NESdev wiki, "FDS BIOS" — <https://www.nesdev.org/wiki/FDS_BIOS>
  - S11: NESdev wiki, "FDS disk format" — <https://www.nesdev.org/wiki/FDS_disk_format>
  - S12: NESdev wiki, "FDS file format" — <https://www.nesdev.org/wiki/FDS_file_format>
- Note: `docs/SOURCES.md` lists no source for general 2C02 rasterization
  timing, scanline/frame structure, sprite evaluation, palette or colour
  generation. Every claim in sections 5-6 that is not pinned by S10 is
  therefore `UNKNOWN` (see section 8). No 2C02 rasterization source is added
  by this document.

No BIOS, ROM, disassembly, or emulator source was consulted. `spec/nes/`
(services.json / README.md) is absent from this checkout.

--------------------------------------------------------------------------------

## 1. Boundary: who owns what

The FDS HLE library (`src/systems/fds/fds_hle.c`) is a 6502 BIOS HLE. It
does not contain a CPU, a PPU, an APU or a disk drive. The host core is
responsible for all of the following; the HLE only reads and writes guest
memory through `ezhle_bus_t`:

- 6502 execution and dispatch of `ezhle_dispatch()` at a BIOS entry address
  (`include/ezhle/ezhle_fds.h`, `EZHLE_FDS_BIOS_BASE`/`EZHLE_FDS_BIOS_SIZE`).
- Guest memory map, including PPU registers (`$2000-$2007` region) and VRAM.
  The HLE never dereferences guest pointers directly (see `ezhle_bus_t`).
- NMI/IRQ vector control bytes at `$0100-$0103` and the dispatch behaviour
  documented in S10 (NMI/IRQ/RESET vectors).
- All 2C02 rasterization: scanline timing, frame buffer, OAM evaluation,
  sprite/background compositing, palette lookup, colour emphasis. None of this
  is defined by this repository or its allowed sources.

The HLE's only PPU-touching surfaces are the documented FDS BIOS entry points
in section 4 and the VRAM transfer structure in section 4.7. Everything else
about rasterization is outside this contract and is `UNKNOWN` (section 8).

--------------------------------------------------------------------------------

## 2. Inputs the host core must provide to the HLE

The HLE dispatch path consumes a single `ezhle_regs_t` plus the descriptor's
bus. Relevant fields (from `include/ezhle/ezhle.h`):

| Input | Type / unit | Meaning | Source |
|---|---|---|---|
| `core` | `ezhle_core_t *` | opaque core handle returned by `ezhle_create()` | `ezhle.h` |
| `regs->r[EZHLE_FDS_REG_A]` (`r[0]`) | uint8 (8-bit) | 6502 accumulator on entry | `ezhle_fds.h` |
| `regs->r[EZHLE_FDS_REG_X]` (`r[1]`) | uint8 (8-bit) | 6502 X index on entry | `ezhle_fds.h` |
| `regs->r[EZHLE_FDS_REG_Y]` (`r[2]`) | uint8 (8-bit) | 6502 Y index on entry | `ezhle_fds.h` |
| `regs->r[EZHLE_FDS_REG_P]` (`r[3]`) | uint8 (8-bit) | 6502 processor status on entry | `ezhle_fds.h` |
| `regs->r[EZHLE_FDS_REG_SP]` (`r[4]`) | uint8 (8-bit) | 6502 stack pointer on entry | `ezhle_fds.h` |
| `regs->pc` | uint32 (16-bit used) | guest PC; for direct-pointer routines the BIOS reads pointers from the instruction stream via the return address | S10 |
| `desc.bus` | `ezhle_bus_t *` | guest memory/log hooks; `read_bytes`/`read32`/`write32`/`log`/`trace` | `ezhle.h` |
| `desc.bus->ctx` | `void *` | opaque context passed to every bus callback | `ezhle.h` |
| attached disk image | bytes (optional) | attached via `ezhle_fds_attach_disk()`; affects only file-count services, not rasterization | `ezhle_fds.h` |

Bus callback contract (from `ezhle.h`): all callbacks return `0` on success
and non-zero on fault. `read32`/`write32` operate on 32-bit words; the HLE
adapts them to 8-bit accesses (see `fds_read8`/`fds_write8` in
`src/systems/fds/fds_hle.c`). If neither `read_bytes` nor `read32` is
provided, the HLE returns `EZHLE_ERR_UNIMPL` for any memory access.

--------------------------------------------------------------------------------

## 3. Outputs the HLE produces

| Output | Type / unit | Meaning | Source |
|---|---|---|---|
| `ezhle_dispatch()` return | `int` | `EZHLE_OK` handled; `EZHLE_ERR_UNIMPL` no handler (`A` set to `$FF`); other negative on fault | `ezhle.h` |
| `regs->r[EZHLE_FDS_REG_A]` | uint8 (8-bit) | guest-visible result for documented routines; `$00` success, non-zero BCD error number where documented | S10 |
| `regs->r[EZHLE_FDS_REG_Y]` | uint8 (8-bit) | routine-specific count where documented (e.g. files loaded) | S10 |
| guest memory writes | bytes | zero-page `$00-$0F` clobbered by disk routines; PPU nametable addresses for `Pixel2NamConv` | S10 |
| `ezhle_last_error()` | `const char *` | last internal error string, if any | `ezhle.h` |
| log callback | text | `bus->log(ctx, level, message)` calls on unimpl/fault paths | `ezhle.h` |

The HLE never writes a framebuffer, never advances a scanline and never
produces audio samples. Those are host-core outputs.

--------------------------------------------------------------------------------

## 4. Documented FDS BIOS PPU-touching surfaces (S10)

These are the only PPU-related behaviours pinned by the allowed sources. Their
status in `FDS_ENTRIES` (`src/systems/fds/fds_hle.c`) is noted.

### 4.1 Power-on / reset initialisation of the PPU

From S10 ("Power-on and reset"):

- Nametable arrangement is set to vertical ("horizontal mirroring").
- PPU registers are initialised, including a warm-up loop.
- Approval check: the 224-byte string at PPU `$2800-$28DF` must match the data
  in the BIOS starting at `$ED37`; the BIOS ensures it is present in the
  nametable and displays it on screen for several seconds after boot files
  load. Error `$20` if the check fails.
- Stack pointer set to `$FF`; APU Frame Counter set to `$C0` (5-step mode,
  disable IRQs); Interrupt Disable flag cleared.
- Zero-page mirrors written on reset (S10, "Zero-page variables"):
  - `$F9` = mirror of `$4026` (value `$FF` on reset).
  - `$FA` = mirror of `$4025` (value `$2E` on reset).
  - `$FB` = mirror of `$4016` (value `$00` on reset).
  - `$FC` = mirror of `$2005/2` (PPUSCROLL, value `$00` on reset).
  - `$FD` = mirror of `$2005/1` (PPUSCROLL, value `$00` on reset).
  - `$FE` = mirror of `$2001` (PPUMASK, value `$06` on reset).
  - `$FF` = mirror of `$2000` (PPUCTRL, value `$10` on reset).
- System RAM is filled with values used by the BIOS; PRG RAM is uninitialised
  except for parts that have files loaded into them.

Units: bytes (addresses), cycles for the warm-up loop. The exact cycle count of
the warm-up loop is not stated in S10 (see section 8).

### 4.2 PPU helper entry points (S10, "Other BIOS calls")

These entries are in `FDS_ENTRIES` with `implemented = 0` (stubs). Per the
current dispatch behaviour they return `EZHLE_ERR_UNIMPL` and set guest
`A = $FF` (FDS-COVERAGE.md). Source: S10.

| Address | Name | Documented input | Documented effect | Status in repo |
|---|---|---|---|---|
| `$E161` | `DisPFObj` | `A`, `$FE` | Disable rendering for both sprites and background | unimpl |
| `$E16B` | `EnPFObj` | `A`, `$FE` | Enable rendering for both sprites and background | unimpl |
| `$E171` | `DisObj` | `A`, `$FE` | Disable sprite rendering | unimpl |
| `$E178` | `EnObj` | `A`, `$FE` | Enable sprite rendering | unimpl |
| `$E17E` | `DisPF` | `A`, `$FE` | Disable background rendering | unimpl |
| `$E185` | `EnPF` | `A`, `$FE` | Enable background rendering | unimpl |

Inputs/outputs beyond the S10 one-line descriptions above are `UNKNOWN`
(carry flag, exact `$FE` bit manipulation, return values). S10 lists only
`A` and `$FE` as inputs for these; no output registers are documented.

### 4.3 Pixel2NamConv (`$E97D`) — implemented

From S10 and `fds_pixel2nam()` in `src/systems/fds/fds_hle.c`:

- Inputs (guest memory):
  - `$02` = pixel Y coordinate (uint8).
  - `$03` = pixel X coordinate (uint8).
- Output (guest memory):
  - `$00` = high byte of nametable address.
  - `$01` = low byte of nametable address.
- Formula: `tile = (Y >> 3) * 32 + (X >> 3)`; address = `$2000 + tile`.
- Guest `A = $00` on success.
- Assumption documented in S10: no scrolling, and the address points at the
  first nametable `$2000-$23FF`. Any scroll offset handling is `UNKNOWN`.
- Error behaviour: if any guest memory access faults, the HLE returns
  `EZHLE_ERR_FAULT` and leaves the affected byte unwritten (no partial
  success is reported).

### 4.4 Nam2PixelConv (`$E997`) — implemented

From S10 and `fds_nam2pixel()` in `src/systems/fds/fds_hle.c`:

- Inputs (guest memory):
  - `$00` = high byte, `$01` = low byte of nametable address.
- Output (guest memory):
  - `$02` = pixel Y coordinate (top-left of the tile, multiple of 8).
  - `$03` = pixel X coordinate (top-left of the tile, multiple of 8).
- Range check: address must be in `$2000-$23FF`; otherwise the HLE returns
  `EZHLE_ERR_FAULT` (no guest writes performed).
- Guest `A = $00` on success.
- Inverse of 4.3 within the documented no-scroll, first-nametable assumption.

### 4.5 VINTWait (`$E1B2`) — unimpl

S10 documents `VINTWait` as a vblank synchronisation gate used before disk I/O
and rendering updates (the example code calls it before `LoadFiles`). It is a
vector-control-driven routine, not a plain JSR target in all cases (see
`FDS-COVERAGE.md`: `VINTWaitHandler` `$E19D` is `unknown`). The host core must
provide vblank/NMI timing for it to be meaningful. In this repository it
returns `EZHLE_ERR_UNIMPL` / `A = $FF`. Its full contract (wait condition,
timeout, register effects) is `UNKNOWN` (see section 8).

### 4.6 VRAMStructWrite (`$E7BB`) and the VRAM transfer structure — unimpl

S10 documents the VRAM transfer structure consumed by the VRAM helpers. The
host core must provide VRAM read/write for these to have an effect. Structure
(from S10, "VRAM transfer structure"):

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | VRAM address (big-endian) |
| 2 | 1 | bits 0-5: length ($0 means 64); bit 6: 0=copy, 1=fill; bit 7: 0=increment by 1, 1=increment by 32 |
| 3 | n | data to copy to VRAM |
| ... | ... | repeated as many times as needed |
| end | 1 | bit 7 = 1 terminates the structure |

Additional documented sub-features (S10): a `$4C` byte calls a sub-VRAM
structure at the following little-endian word; `$60` returns from a
sub-structure. "Load Tileset notes" documents a 7-bit tile-load header
(`AAAA MMIT`: fill bit, transfer direction, bitplane type, low VRAM address)
with four bitplane types (normal 2-bitplane; three single-bitplane variants).

Entry-point status in `FDS_ENTRIES`:
- `$E7BB` `VRAMStructWrite` — unimpl.
- `$E844` `FetchDirectPtr` — unimpl.
- `$E86A` `WriteVRAMBuffer` — unimpl.
- `$E8B3` `ReadVRAMBuffer` — unimpl.
- `$E8D2` `PrepareVRAMString` — unimpl.
- `$E8E1` `PrepareVRAMStrings` — unimpl.
- `$E94F` `GetVRAMBufferByte` — unimpl.

Their per-routine input/output contracts beyond the structure format above are
`UNKNOWN` (see section 8).

### 4.7 Approval check and boot display — host core must render it

S10 requires the 224-byte string at PPU `$2800-$28DF` to match BIOS data at
`$ED37`, and the BIOS displays it for several seconds after boot files load.
The host core must therefore provide PPU rendering capable of showing a
nametable at `$2800` for the boot/approval screen. Timing of "several
seconds" is not quantified in S10 (see section 8).

--------------------------------------------------------------------------------

## 5. Units

Documented units, linked to their source:

| Quantity | Unit | Value / rule | Source |
|---|---|---|---|
| Guest addresses | bytes (16-bit CPU address) | `$0000-$FFFF`; PPU nametable region `$2000-$23FF` for `Pixel2NamConv` | S10 |
| VRAM addresses in transfer structure | bytes (16-bit) | big-endian in the structure | S10 |
| File IDs / counts | count (0-255) | up to 20 IDs per list | S10 |
| `Delay131` duration | CPU cycles | 131 cycles (no-op in HLE) | S10, `fds_delay131` |
| Approval string length | bytes | 224 bytes at PPU `$2800-$28DF` | S10 |
| VRAM transfer length field | bytes | bits 0-5; `$0` means 64 | S10 |
| Scanline / frame timing | (unit) | `UNKNOWN` — no 2C02 rasterization source in `docs/SOURCES.md` | section 8 |
| Frame rate | frames/second | `UNKNOWN` | section 8 |
| CPU master clock | Hz | `UNKNOWN` | section 8 |
| Audio sample rate / samples | samples/second | `UNKNOWN` — audio is outside this HLE; no audio source listed | section 8 |
| Input-to-photon latency | frames / ms | `UNKNOWN` | section 8 |

The HLE itself uses no wall clock in any guest-visible path (project rule);
all timing it exposes is count-based (e.g. the two-attempt retry in
`LoadFiles`).

--------------------------------------------------------------------------------

## 6. Error behaviour

### 6.1 HLE-internal error space

From `include/ezhle/ezhle.h`:

| Code | Value | Meaning |
|---|---|---|
| `EZHLE_OK` | 0 | handled |
| `EZHLE_ERR_ARG` | -1 | bad argument |
| `EZHLE_ERR_NOMEM` | -2 | out of memory |
| `EZHLE_ERR_UNIMPL` | -3 | no handler / undocumented |
| `EZHLE_ERR_FAULT` | -4 | bus fault or invalid guest state |
| `EZHLE_ERR_STATE` | -5 | state error (e.g. image parse) |

These are internal and are **not** guest-visible except through the `A`
translation in 6.2.

### 6.2 Guest-visible `A` for PPU-touching surfaces

| HLE outcome | Guest `A` | Notes |
|---|---|---|
| `Pixel2NamConv` / `Nam2PixelConv` success | `$00` | documented by S10 |
| `Pixel2NamConv` / `Nam2PixelConv` bus fault | unchanged guest `A`? | HLE returns `EZHLE_ERR_FAULT`; guest `A` is not set to `$FF` (see `fds_pixel2nam`/`fds_nam2pixel`) |
| `Nam2PixelConv` out-of-range address (`<$2000` or `>$23FF`) | guest `A` not set | HLE returns `EZHLE_ERR_FAULT` without writing `$02/$03` |
| Unimplemented PPU helper (`DisPFObj` etc., `VRAMStructWrite` etc., `VINTWait`) | `$FF` | stub fallback documented in FDS-COVERAGE.md |
| No disk for file-count services | `$FF` | `EZHLE_ERR_UNIMPL` (not a rasterization case) |

Bus-fault to BIOS-error-code mapping for PPU paths is `UNKNOWN`: S10 does not
define a BIOS error code for a bad PPU/VRAM access, so the HLE must not
invent one (it returns `EZHLE_ERR_FAULT` internally).

### 6.3 Host-core rasterization errors

The HLE has no visibility into rasterization. If the host core cannot render
(e.g. missing VRAM, bad PPU state), that is outside this contract; the HLE
does not translate it. Any such behaviour is `UNKNOWN` here.

--------------------------------------------------------------------------------

## 7. Host-core obligations (summary)

For FDS BIOS compatibility the host core must provide, at minimum:

1. Guest memory bus implementing `ezhle_bus_t` (`read_bytes` or `read32` plus
   `write32`), with fault reporting (non-zero on fault).
2. 6502 registers passed in `ezhle_regs_t` and PC handling, including the
   direct-pointer convention for disk routines (S10).
3. NMI/IRQ/RESET vector control bytes `$0100-$0103` and their dispatch (S10).
4. PPU register mirrors at zero-page `$FC-$FF` and the reset values above (S10).
5. VRAM accessible at the nametable addresses used by `Pixel2NamConv`
   (`$2000-$23FF`) and the approval string region `$2800-$28DF` (S10).
6. Vblank/NMI timing so `VINTWait` (when implemented) can synchronise; the
   exact timing is `UNKNOWN` (section 8).
7. Rendering of the boot/approval screen nametable (S10).

The host core's rasterization quality (frame rate, scanline timing, sprite
limits, palette) is not constrained by this document because no allowed source
defines it; see section 8.

--------------------------------------------------------------------------------

## 8. UNKNOWN (must not be guessed)

These behaviours are not pinned by the allowed sources (`docs/SOURCES.md`).
The HLE must not infer them from another emulator. Until a source documents
them, the affected BIOS entry points stay `unimpl` and any rasterization detail
remains host-defined:

1. 2C02 scanline count, frame structure and vblank timing (no PPU source in
   `docs/SOURCES.md`).
2. Frame rate (NTSC/PAL) and CPU master clock ratio.
3. Sprite evaluation limits, sprite overflow flag, OAM decay.
4. Palette, colour emphasis, and composite-video generation.
5. Mirroring beyond the reset value "vertical (horizontal mirroring)" stated
   in S10.
6. Warm-up loop cycle count (S10 says it exists but gives no count).
7. "Several seconds" display duration for the approval screen (S10).
8. Carry flag, `X` and exact zero-page clobber for `DisPFObj`/`EnPFObj`/
   `DisObj`/`EnObj`/`DisPF`/`EnPF`.
9. Whether `DisPF`/`EnPF` etc. modify `$2001` directly or only the `$FE`
   mirror.
10. `VINTWait` wait condition, timeout, and register effects.
11. `VRAMStructWrite`/`WriteVRAMBuffer`/`ReadVRAMBuffer`/`PrepareVRAMString`/
    `PrepareVRAMStrings`/`GetVRAMBufferByte`/`FetchDirectPtr` per-routine
    input/output contracts beyond the structure format in 4.6.
12. `Pixel2NamConv`/`Nam2PixelConv` behaviour with scrolling or nametables
    other than `$2000-$23FF` (S10 assumes no scrolling).
13. Bus-fault to BIOS-error-code mapping for any PPU/VRAM access (no
    documented code).
14. Whether the host core exposes rasterization errors to the HLE at all (it
    does not, by design).
15. Audio: sample rate, buffering, underrun handling — no audio source is
    listed in `docs/SOURCES.md`; outside this contract.

--------------------------------------------------------------------------------

## 9. HLE stub mapping obligations

Until the unimplemented PPU-touching entries are implemented, the dispatch path
(`fds_dispatch` in `src/systems/fds/fds_hle.c`) returns `EZHLE_ERR_UNIMPL` and
sets guest `A = $FF` for every unimpl entry (current, documented behaviour —
FDS-COVERAGE.md). This is the stub fallback and applies to `DisPFObj`,
`EnPFObj`, `DisObj`, `EnObj`, `DisPF`, `EnPF`, `VINTWait`,
`VRAMStructWrite`, `FetchDirectPtr`, `WriteVRAMBuffer`, `ReadVRAMBuffer`,
`PrepareVRAMString`, `PrepareVRAMStrings`, `GetVRAMBufferByte`.

When any of these is implemented, it must stop using `A = $FF` for the
documented success/error cases and use the BIOS error codes pinned by S10 where
applicable. Any behaviour not pinned by S10 stays `EZHLE_ERR_UNIMPL`.

--------------------------------------------------------------------------------

## Sources

- S10 — NESdev wiki, "FDS BIOS": <https://www.nesdev.org/wiki/FDS_BIOS>
  (sections: "Power-on and reset", "Zero-page variables",
  "Interrupt/Reset vector controls", "Disk access routines",
  "Other BIOS calls" / "VRAM transfer structure" / "Load Tileset notes" /
  "Object structure", "Example code to load files").
- S11 — NESdev wiki, "FDS disk format": <https://www.nesdev.org/wiki/FDS_disk_format>
  (disk info block offsets used by the approval-check and Disk-ID paths).
- S12 — NESdev wiki, "FDS file format": <https://www.nesdev.org/wiki/FDS_file_format>
  (multi-side ordering; file types including nametable files).

NESdev wiki text is used as a specification reference only; no wiki text is
reproduced verbatim in this document beyond short, necessary quotations marked
as such. No BIOS, ROM, disassembly, key, or leaked SDK was consulted.
`docs/SOURCES.md` contains no entry for general 2C02 rasterization; that
absence is the reason section 8 marks those details `UNKNOWN`.
