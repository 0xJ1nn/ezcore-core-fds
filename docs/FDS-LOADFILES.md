# FDS-107 — LoadFiles (`$E1F8`) contract

Status: **contract document — no code, no tests.** This pins the LoadFiles
service from public documentation only (NESdev wiki). Anything the wiki does
not document is marked `UNKNOWN` and must not be guessed; until a behaviour is
documented, the HLE must report `EZHLE_ERR_UNIMPL`.

- Entry address: `$E1F8` (name `LoadFiles`, `implemented = 0` in `FDS_ENTRIES`).
- Current HLE behaviour: stub — returns `EZHLE_ERR_UNIMPL`, `A = $FF`, reason
  logged and stored in `ezhle_last_error()`. No guest memory is read/written.
- Sources (public documentation only):
  - S10: NESdev wiki, "FDS BIOS" — <https://www.nesdev.org/wiki/FDS_BIOS>
  - S11: NESdev wiki, "FDS disk format" — <https://www.nesdev.org/wiki/FDS_disk_format>
  - S12: NESdev wiki, "FDS file format" — <https://www.nesdev.org/wiki/FDS_file_format>

No BIOS, ROM, disassembly, or emulator source was consulted and none was
available in this tree. `spec/nes/` (services.json / README.md) is absent from
this checkout; this document follows FDS-107's mandated sources.

--------------------------------------------------------------------------------

## 1. Entry address

`$E1F8`. Row in `FDS_ENTRIES` (`src/systems/fds/fds_hle.c:246`):
`{ 0xE1F8u, "LoadFiles", 0 }`.

--------------------------------------------------------------------------------

## 2. Calling convention — direct pointers

LoadFiles is a member of the FDS BIOS "Disk access routines". Per the wiki
(S10, "Disk access routines" section):

> These routines take one or two direct pointers as arguments. They must be
> placed directly after the JSR instruction: the return address in the stack is
> used to fetch the pointers and is subsequently fixed. Zero-page memory at
> `$00-$0F` will be affected by these routines.

Reference invocation (S10, example code to load files):

```
        JSR VINTWait
        JSR LoadFiles
        .dw DiskID      ; pointer #1 (little-endian 16-bit)
        .dw LoadList    ; pointer #2 (little-endian 16-bit)
        BNE _Error
        RTS
```

So the input pointers are **not** zero-page register inputs; they are two
16-bit addresses embedded in the instruction stream after the `JSR`, read by
the BIOS through the return address on the stack. The return address is then
repaired so `RTS` lands past both pointer words.

--------------------------------------------------------------------------------

## 3. Input contract

| Input | Meaning | Source |
|---|---|---|
| Direct pointer #1 (word at the `JSR` return address) | Address of the Disk ID structure (10 bytes) | S10, "Disk access routines" + LoadFiles row |
| Direct pointer #2 (word at return address + 2) | Address of the File List (byte string) | S10, LoadFiles row |
| Accumulator `A` on entry | UNKNOWN — not documented as an input | S10 (silent) |
| `X` on entry | UNKNOWN — not documented as an input | S10 (silent) |
| `Y` on entry | UNKNOWN — not documented as an input | S10 (silent) |
| Processor status `P` on entry | UNKNOWN — not documented as an input | S10 (silent) |
| Zero-page `$00-$0F` on entry | UNKNOWN — the wiki only states this range is *affected (clobbered)* on output, not read on input | S10 |
| `[$0101]` (IRQ vector-control byte) | Preserved on entry, restored on exit | S10, "Disk access routines" preamble |
| Non-disk IRQs (timer, DMC, APU frame) | Caller must disable before the call | S10, "Disk access routines" preamble |
| NMIs | Caller must disable before the call (e.g. via `JSR VINTWait`) | S10, "Disk access routines" preamble |

### Zero-page clobber
The wiki states `$00-$0F` "will be affected" by these routines. Which exact
bytes are used by LoadFiles, and what they are set to, is **UNKNOWN**.

### Caller prerequisites
Documented. LoadFiles is not required to itself disable IRQs/NMIs; the calling
program is responsible for:
- disabling all non-disk IRQ sources (timer, DMC, APU frame counter), and
- disabling NMIs (e.g. `JSR VINTWait`) before the call.

--------------------------------------------------------------------------------

## 4. File-list format and terminator

Source: S10, "File List structure" section and the LoadFiles row.

- A **byte string** of 1-byte file IDs.
- Each byte is a file ID to match against files on the disk. **All files that
  match any ID in the list are loaded** (a file matching multiple list entries
  is loaded once).
- A list of up to **20 IDs** is possible at a time.
- Shorter lists are **terminated by `$FF`**. A file ID can therefore never be
  `$FF` (this implies `$FF` is reserved as the terminator).
- Files are loaded in **disk order** (the order they exist on the disk), not in
  the order of the list.

### Boot-files mode (special terminator case)
If the File List consists of **only the `$FF` terminator** (an empty list), then
LoadFiles does **not** load nothing. Instead it loads every file whose file ID
is **less than or equal to the boot read file code** found in the disk info
block at offset `$19` (S11). The wiki notes this mode "is used by the BIOS
during startup."

### Max-20 boundary
UNKNOWN whether a list of exactly 20 IDs (no `$FF` terminator) is accepted, or
whether the terminator is mandatory and 19 is the practical ceiling. The wiki
states "up to 20 IDs is possible" and "shorter lists should be terminated by a
$ff byte", but does not enumerate the exact full-list boundary behaviour.

--------------------------------------------------------------------------------

## 5. Disk ID structure

Source: S10 "Disk ID structure" + S11 disk info block ($0F..$18).

The Disk ID is the 10 bytes at disk-info-block offsets `$0F..$18` (i.e. the
10 bytes immediately following the `*NINTENDO-HVC*` string). LoadFiles passes
this structure to CheckDiskHeader, which compares each byte against the disk;
an `$FF` byte in any field skips that field's comparison (wildcard).

| DiskID offset | Disk-info offset | Size | Field | CheckDiskHeader error if mismatch |
|---|---|---|---|---|
| 0 | `$0F` | 1 | Licensee code | `$04` |
| 1 | `$10..$13` | 4 | Game ASCII name (3 letters) + game type (1 byte) | `$05` |
| 5 | `$14` | 1 | Game version | `$06` |
| 6 | `$15` | 1 | Disk side (`A`=`$00`, `B`=`$01`) | `$07` |
| 7 | `$16` | 1 | Disk number (`$00` = first disk) | `$08` |
| 8 | `$17` | 1 | Disk type | `$09` |
| 9 | `$18` | 1 | Unknown (`$00` in all known games) | `$10` |

Boot-time behaviour (S10): "the ROM BIOS boots a disk... sets all the fields in
the DiskID string to `$FF` except for disk side #, and disk #, which are set to
0."

--------------------------------------------------------------------------------

## 6. File header (per-file load description)

Source: S11, "File header block (block 3)". LoadFiles reads each matched
file's header to decide where and how to place the data; "load addresses are
decided by the file's header" (S10).

| Offset in block 3 | Size | Field |
|---|---|---|
| `$00` | 1 | Block code `$03` |
| `$01` | 1 | File number (not used by the BIOS during reads) |
| `$02` | 1 | File ID (matched against the File List) |
| `$03` | 8 | File name |
| `$0B` | 2 | Load address (16-bit LE) — destination in CPU memory |
| `$0D` | 2 | File size (16-bit LE) |
| `$0F` | 1 | File type (`0`=Program/PRG-RAM, `1`=Character/CHR-RAM, `2`=Nametable/VRAM) |

--------------------------------------------------------------------------------

## 7. Output / error convention

Source: S10, "Disk access routines" preamble + the LoadFiles table row.

On return:

| Register | Documented meaning |
|---|---|
| `A` | `$00` = no error; any non-zero value is the **error number** (BCD). Test with `BEQ` (branch if no error) / `BNE` (branch if error). |
| `Y` | Number of files loaded. |
| Carry | **UNKNOWN** — the wiki never mentions the carry flag for the disk access routines; only `A` and `Y` are described. Must not be guessed. |
| `X` | UNKNOWN — not documented as an output. |
| Zero-page `$00-$0F` | Affected (clobbered); exact bytes UNKNOWN. |

### Retry semantics
The wiki states LoadFiles loads "attempting twice before returning any errors."
This is a **fixed two-attempt** sequence (deterministic), not a wall-clock
timeout. The HLE must not use real time for the retry or for any guest-visible
value (project rule: no wall clock in guest-visible paths). The exact conditions
under which the second attempt is taken (e.g. whether a no-disk condition
short-circuits before both attempts, which error classes trigger the retry,
whether the two attempts target the same side) are **UNKNOWN**.

### Output of registers on a faulting bus access
When the HLE's bus callback faults (e.g. the Disk-ID or File-List pointer
points outside guest memory), the HLE currently has no mapping to a BIOS error
code. Which BIOS error code represents a bad guest pointer is **UNKNOWN**; it
must not be guessed. See section 12.

--------------------------------------------------------------------------------

## 8. Error codes

Source: S10, "Error list" (BCD numbers "returned by the BIOS disk I/O
routines"). LoadFiles is a disk I/O routine; the table below lists the full
documented set. The wiki gives a *general* error list but **does not enumerate a
per-routine error map**, so for each scenario (section 9) we cite the code(s) the
wiki associates with that cause and mark the rest UNKNOWN.

| Code (BCD) | Message / explanation | LoadFiles relevance |
|---|---|---|
| `$00` | No error | success |
| `$01` | disk set (`$4032.0`) — Disk not set | no disk inserted (disk I/O routine) |
| `$02` | battery (`$4033.7`) — Power supply failure | UNKNOWN if LoadFiles surfaces it |
| `$03` | (`$4032.2`) — Disk is write protected | write path; likely not LoadFiles |
| `$04` | Wrong licensee code | Disk ID `[$0F]` mismatch (CheckDiskHeader) |
| `$05` | Wrong game ASCII name string or game type | Disk ID `[$10..$13]` mismatch |
| `$06` | Wrong game version | Disk ID `[$14]` mismatch |
| `$07` | a,b side — Wrong side number | Disk ID side `[$15]` mismatch |
| `$08` | disk no. — Wrong disk number | Disk ID disk# `[$16]` mismatch |
| `$09` | Wrong disk type | Disk ID type `[$17]` mismatch |
| `$10` | Wrong value for unknown field | Disk ID `[$18]` mismatch |
| `$20` | disk trouble — Approval check failed | license-screen check (boot); UNKNOWN for LoadFiles |
| `$21` | disk trouble — `*NINTENDO-HVC*` doesn't match | Disk info block verification |
| `$22` | disk trouble — Disk info block expected | structural |
| `$23` | disk trouble — File amount block expected | structural |
| `$24` | disk trouble — File header block expected | structural |
| `$25` | disk trouble — File data block expected | structural |
| `$26` | disk trouble — AppendFile/WriteFile readback verify failed | write path |
| `$27` | disk trouble — Block failed CRC (`4030.4`) | CRC (omitted from `.FDS`); UNKNOWN for LoadFiles |
| `$28` | disk trouble — File ends prematurely during read (disk full) | read path |
| `$29` | disk trouble — File ends prematurely during write (disk full) | write path |
| `$30` | disk trouble — File ends prematurely during CRC writes | CRC write path |
| `$31` | disk trouble — CheckFileCount/AdjustFileCount: count too high | count routines |
| `$35` | Failed to write test file (block 5)? | write path; trailing `?` in wiki (unconfirmed) |
| `$40` | LoadFiles: Could not load all files from the File List | LoadFiles-specific |
| `$41~` | Unknown (appears in manual for Kineko II) | UNKNOWN |

Notes:
- `$27` (CRC) is physically produced by the RP2C33 drive controller, not the
  CPU, and CRCs are omitted from the `.FDS` container (S11). Whether the HLE
  must ever surface `$27` for an in-memory image is UNKNOWN.
- `$35` is printed in the wiki with a trailing `?`, i.e. its applicability is
  itself unconfirmed.
- The wiki does **not** state that LoadFiles returns `$02` (power) or `$03`
  (write-protected) for reads; those are write/current-source paths. Treating
  them as applicable to LoadFiles would be an inference — mark UNKNOWN.

--------------------------------------------------------------------------------

## 9. Scenarios

### 9.1 No disk inserted
Documented. Error `$01` ("disk set (`$4032.0`) Disk not set"). LoadFiles is a
disk I/O routine that reads the disk; with no disk inserted `$4032.0` is set and
the routine returns `$01`. The HLE (no physical drive) should therefore report
`$01` when no image is attached — **not** the generic `EZHLE_ERR_UNIMPL`/`A=$FF`
stub fallback, because the wiki *does* pin a guest-visible error channel for
LoadFiles (`A` = error#). The two-attempt retry still applies (both attempts
fail); the exact short-circuit order is UNKNOWN (see 7).

> Contrast: GetNumFiles/SetNumFiles have no documented guest-visible error
> channel for the no-disk case, so they fall back to `EZHLE_ERR_UNIMPL`/`A=$FF`
> (see FDS-COVERAGE.md). LoadFiles is different because `A=error#` is
> documented.

### 9.2 Missing file (a File-List ID that no file on the disk matches)
Documented-by-description. `$40` — "LoadFiles: Could not load all files from the
File List." An unmatched ID means not every file in the list was loaded, so `$40`
is the documented incomplete-load error. **UNKNOWN** whether an unmatched ID is
reported as `$40` while still loading the matched files (Y = files actually
loaded), or whether the routine aborts at the first unmatched ID; the wiki gives
no per-cause breakdown of `$40`.

### 9.3 Bad load address
Documented. The wiki (S10, "Disk access routines" preamble) states:

> Loads to memory locations below `$2000` are ignored but the `$x200-$0x7FF`
> area is permitted. ... Dummy loads are used (i.e. file contents are ignored)
> if the destination address is below CPU address `$2000` (excluding the
> `$x200-$x7FF` area), or if the destination address plus file size would cause a
> 16-bit overflow.

Concrete address rules:
- Load address `$0000-$01FF` → dummy load (ignored), no error.
- Load address `$0200-$07FF` (and its mirror `$1200-$17FF` via bit masking) →
  permitted (loaded).
- Load address `$0800-$1FFF` → dummy load (ignored), no error.
- Load address `>= $2000` → loaded.
- `load address + file size > $FFFF` (16-bit overflow) → dummy load (ignored),
  no error.

The wiki describes these as "ignored"/"dummy" loads but **never** states an error
code is raised for them; the documented reading is that they are silently
skipped. Whether any internal error flag is recorded is UNKNOWN.

### 9.4 Multi-side image
Partially documented. The Disk ID structure includes the disk side (`$15`,
error `$07` on mismatch) and disk number (`$16`, error `$08` on mismatch). LoadFiles
compares the Disk ID against the **currently inserted** side only; it does not, per
documented behaviour, auto-advance the drive to another side. The reference
example (S10) polls `$4032` bit 0 (disk-set / eject) around disk operations that
may span sides:

```
_sideError: LDA $4032 AND #$01
            BEQ _sideError   ; wait until disk ejected
_insert:    LDA $4032 AND #$01
            BNE _insert      ; wait until disk inserted
            JMP Load
```

If the Disk ID's side/disk-number does not match the inserted side, CheckDiskHeader
returns `$07`/`$08`. Beyond that, **UNKNOWN**:
- Whether LoadFiles auto-advances to the next side of a multi-`$0F`-block image
  (it does not, per the documented game-loop model, but this is by example).
- The HLE has no drive/side model; its "current side" is the first side of the
  attached image (see FDS-COVERAGE.md). Multi-side ejection/insertion and side
  selection are **UNKNOWN** / out of contract scope for this slice.

--------------------------------------------------------------------------------

## 10. Determinism / state save-load

Project rules (not wiki-derived):
- State save/load must be **deterministic**; LoadFiles operates on the attached
  in-memory image copy, so its observable result is a pure function of (a) the
  attached image, (b) the Disk ID and File List contents, and (c) CPU memory
  state at the destination addresses. There is no wall clock in this path.
- The two-attempt retry (section 7) must be a **count-based** retry over the same
  in-memory side, never a time-based spin.
- `Y` (# files loaded) and the written memory must be reproducible across
  identical inputs on every call.

--------------------------------------------------------------------------------

## 11. UNKNOWN (must not be guessed)

These behaviours are not pinned by the wiki. The HLE must **not** infer them
from another emulator. Until a source documents them, treat as `EZHLE_ERR_UNIMPL`:

1. Carry flag on return (see 7).
2. Exact zero-page bytes in `$00-$0F` clobbered, and their values (see 3).
3. `A`/`X`/`Y`/`P` input values consumed (see 3).
4. `Y` value on a partial/error load (does it count files loaded before the
   failure?) (see 7).
5. The two-attempt retry ordering: does no-disk short-circuit before both
   attempts; which error classes trigger the retry; does the retry span sides (see 7, 9.1).
6. Full per-routine error map: the wiki lists error *codes* generally but does
   not state which each routine returns for each cause (see 8).
7. Whether an unmatched File-List ID yields `$40` while still loading matches,
   or aborts early (see 9.2).
8. Whether "dummy loads" (bad address / overflow) record any error flag (see 9.3).
9. Whether LoadFiles auto-advances drive sides for multi-side disks (example
   shows a game-side eject/insert loop) (see 9.4).
10. Bus-fault → BIOS-error-code mapping: a bad guest pointer (Disk-ID/File-List
    pointer out of range) has no documented BIOS error code (see 7).
11. `$27` (CRC failure) handling: CRCs are drive-controller-produced and omitted
    from `.FDS`, so whether the HLE must ever surface `$27` is UNKNOWN (see 8).
12. `$35` applicability (wiki prints it with a trailing `?`) (see 8).
13. `$41~` cause (wiki: "appears in manual for Kineko II") (see 8).
14. The exact full-list boundary: 20 IDs without a `$FF` terminator accepted or
    not (see 4).

--------------------------------------------------------------------------------

## 12. HLE stub mapping obligations

Until LoadFiles is implemented, the dispatch path
(`src/systems/fds/fds_hle.c`, `fds_dispatch`) returns `EZHLE_ERR_UNIMPL` and
sets guest `A = $FF` for every unimpl entry (current, documented behaviour —
FDS-COVERAGE.md). This is the stub fallback.

When LoadFiles is implemented, it must **stop** using `A=$FF` for the
documented error cases and instead return the BIOS error codes pinned in
sections 7–9 (e.g. `A=$00` + `Y=n` on success; `A=$01` on no disk; `A=$40` on
incomplete load; `A=$04`–`$10` on Disk-ID mismatch). Specifically:

| HLE outcome | Guest `A` | Guest `Y` | Notes |
|---|---|---|---|
| Success | `$00` | files loaded | BEQ path |
| No disk | `$01` | UNKNOWN | not the `$FF` stub fallback |
| Disk-ID mismatch (field n) | `$04`..`$10` | UNKNOWN | per 5.x |
| Incomplete load (e.g. unmatched ID) | `$40` | UNKNOWN | per 9.2 |
| Bus fault (bad guest pointer) | UNKNOWN | UNKNOWN | no documented code; must not guess |
| Unimplemented (future sub-feature) | `$FF` | — | only the stub/unimpl fallback |

The HLE error-space (`EZHLE_ERR_ARG`, `EZHLE_ERR_UNIMPL`, `EZHLE_ERR_FAULT`,
`EZHLE_ERR_STATE`) is internal and is **not** guest-visible except via the
`A` translation above. Mapping an internal `EZHLE_ERR_FAULT` to a BIOS error
code is UNKNOWN (see 11.10) and must not be guessed.

--------------------------------------------------------------------------------

## Sources

- S10 — NESdev wiki, "FDS BIOS": <https://www.nesdev.org/wiki/FDS_BIOS>
  (sections: "Disk access routines", "Low-Level Disk access routines",
  "Example code to load files", "Disk ID structure", "File List structure",
  "File Header structure", "Error list").
- S11 — NESdev wiki, "FDS disk format": <https://www.nesdev.org/wiki/FDS_disk_format>
  (sections: "Disk info block (block 1)", "File amount block (block 2)",
  "File header block (block 3)", "File data block (block 4)").
- S12 — NESdev wiki, "FDS file format": <https://www.nesdev.org/wiki/FDS_file_format>
  (multi-side ordering and side capacity).

NESdev wiki text is used as a specification reference only; no wiki text is
reproduced verbatim in this document beyond short, necessary quotations marked
as such. No BIOS, ROM, disassembly, key, or leaked SDK was consulted.
