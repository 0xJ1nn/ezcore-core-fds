# FDS-AUD-002 — BLOCKER: FDS expansion audio (wavetable) channel

Status: **blocked — missing host hook.** No code change is made beyond this
document and its accompanying test.

## Missing hook

The exact missing hook is an **APU / expanded-audio (wavetable) channel
interface in the host core and libretro frontend**. This repository is
`ezcore-core-fds`, an HLE *library* package (`kind: hle-library`,
`provides: [bios-hle]`); its only host-facing surface is:

- `ezhle_bus_t` (`include/ezhle/ezhle.h`) — guest memory read/write bytes
  plus `log`/`trace` callbacks;
- `ezhle_desc_t` — system identity, bus pointer, optional scheduler intent;
- `ezhle_fds_entry_t` / `ezhle_fds_call()` — documented FDS BIOS entry
  points (6502 routines at `$E000-$FFFF`).

There is no audio register surface, no sample-output callback, no channel
state struct, no clock/timing hook, and no libretro audio callback anywhere in
this tree. The FDS wavetable expansion channel is a hardware/APU feature, not a
documented FDS BIOS entry point, so it cannot be delivered as an HLE service
in this package.

## Consequence

No audio behaviour is faked or partially implemented. Any FDS audio request
remains outside the scope of this library and is reported as unimplemented by
the host core that owns the APU.

## Where it belongs

The wavetable channel implementation belongs in the ezCORE core/APU and
libretro frontend repositories (`ezcore`), not in `ezcore-core-fds`. This
repository should continue to provide only the FDS BIOS HLE and disk-image
parser.

## Test

`tests/test_fds_aud_002.c` records the absence: the FDS entry table contains no
audio/wavetable/sound service, and the public ABI exposes no audio hook.
