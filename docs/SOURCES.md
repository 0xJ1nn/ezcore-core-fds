# Sources

All behavior implemented in this repository must cite one of these public
sources. A source that is not listed here cannot justify an implementation.

| ID | Source | URL | Use |
|---|---|---|---|
| S10 | NESdev wiki — FDS BIOS | https://www.nesdev.org/wiki/FDS_BIOS | entry points, register effects, LoadFiles contract |
| S11 | NESdev wiki — FDS disk format | https://www.nesdev.org/wiki/FDS_disk_format | disk header, file count, file table, sides |
| S12 | NESdev wiki — FDS file format | https://www.nesdev.org/wiki/FDS_file_format | file headers, IDs, data layout |

## Not sources

- Nintendo FDS BIOS images, game disks, keys, or leaked SDK material.
- Disassembly of the above.
- GPL/non-commercial emulator source as copied code. Such projects may be used
  only as a black-box oracle outside this tree; their source is not read here.

## Rule

If the wiki is silent, the behavior is `UNKNOWN`. The implementation returns
`EZHLE_ERR_UNIMPL` and logs the call. A missing fact is never filled in from
memory or from another emulator.
