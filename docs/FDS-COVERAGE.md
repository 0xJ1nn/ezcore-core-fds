# FDS entry-point coverage matrix

Audit of every entry in `FDS_ENTRIES` (`src/systems/fds/fds_hle.c`).
Source of truth: the table in `fds_hle.c` (36 rows). No entries invented.
Public documentation used: NESdev wiki FDS BIOS page (see Citation).
Status values: `implemented`, `unimpl`, `unknown`.

Counts: 5 implemented, 27 unimpl, 4 unknown. Total 36.

| Address | Name | Status | Behaviour implemented | Citation | Test |
|---|---|---|---|---|---|
| 0xE149 | Delay131 | implemented | 131-cycle delay (no-op in HLE) | https://www.nesdev.org/wiki/FDS_BIOS | test_implemented_entry, test_fds_call_helper |
| 0xE153 | Delayms | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE161 | DisPFObj | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE16B | EnPFObj | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE171 | DisObj | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE178 | EnObj | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE17E | DisPF | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE185 | EnPF | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE19D | VINTWaitHandler | unknown | none (UNKNOWN: NMI vector-control target, not a JSR routine; direct-call contract not pinned) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE1B2 | VINTWait | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE1CE | DiskTransfer | unknown | none (UNKNOWN: IRQ vector-control target; direct-call contract not pinned) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE1D9 | DiskSkipBytes | unknown | none (UNKNOWN: IRQ vector-control target; direct-call contract not pinned) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE1EF | AcknowledgeDelay | unknown | none (UNKNOWN: IRQ vector-control target; direct-call contract not pinned) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE1F8 | LoadFiles | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE237 | AppendFile | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE239 | WriteFile | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE2B7 | CheckFileCount | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE2BB | AdjustFileCount | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE301 | SetFileCount1 | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE305 | SetFileCount | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE32A | GetDiskInfo | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE445 | CheckDiskHeader | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE484 | GetNumFiles | implemented | writes attached image's first-side file count to $06; no disk -> UNIMPL + log | https://www.nesdev.org/wiki/FDS_BIOS | test_get_num_files |
| 0xE492 | SetNumFiles | implemented | writes A to the attached image's file count; no disk -> UNIMPL + log | https://www.nesdev.org/wiki/FDS_BIOS | test_set_num_files_roundtrip |
| 0xE4A0 | FileMatchTest | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE4DA | SkipFiles | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE7BB | VRAMStructWrite | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE844 | FetchDirectPtr | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE86A | WriteVRAMBuffer | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE8B3 | ReadVRAMBuffer | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE8D2 | PrepareVRAMString | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE8E1 | PrepareVRAMStrings | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE94F | GetVRAMBufferByte | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |
| 0xE97D | Pixel2NamConv | implemented | pixel coords ($02=Y,$03=X) -> nametable address at $2000 into $00/$01; returns 0 | https://www.nesdev.org/wiki/FDS_BIOS | test_implemented_entry, test_errprop_read, test_errprop_write |
| 0xE997 | Nam2PixelConv | implemented | nametable address $00/$01 -> pixel coords $02/$03; range-checked to $2000-$23FF | https://www.nesdev.org/wiki/FDS_BIOS | test_range_nam2pixel, test_nam2pixel_cov |
| 0xE9B1 | Random | unimpl | none (returns EZHLE_ERR_UNIMPL) | https://www.nesdev.org/wiki/FDS_BIOS | none |

## What a game needs next

Top 5 services by boot importance (currently unimpl):

1. `LoadFiles` ($E1F8) — primary disk file loader; without it a title cannot pull program/data files off disk.
2. `VINTWait` ($E1B2) — vblank synchronization gate used before disk I/O and rendering updates.
3. `VRAMStructWrite` ($E7BB) — VRAM transfer structure writer needed to bring tiles/nametables on screen.
4. `WriteVRAMBuffer` ($E86A) — buffered VRAM transfer path used during vblank for graphics updates.
5. `GetDiskInfo` ($E32A) — disk identification/validation step that boot and file matching depend on.

No game boot is claimed by this document. All unimpl/unknown entries continue to return `EZHLE_ERR_UNIMPL` and log; no stub reports success.
