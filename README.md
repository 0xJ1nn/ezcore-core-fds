# ezCORE Core — FDS HLE

An installable **HLE library package** for the Famicom Disk System, built by
ezCORE HLE Labs. No BIOS, no firmware, no ROM, no key.

**Status: `0.1.1` lab.** This is a library, not a playable libretro core. It
implements 3 of the 36 documented FDS BIOS entry points plus a clean-room FDS
disk image parser. Everything else returns `EZHLE_ERR_UNIMPL` and logs; nothing
is faked.

## What is in the box

```
include/ezhle/ezhle.h              public ABI (ezhle-2)
include/ezhle/ezhle_fds.h          FDS entry-point table and call helper
include/ezhle/ezhle_fds_image.h    .fds image parser
src/                               FDS-only source, PS2 stubbed out
lib/libezhle.a                     static library built from src/, BSD-2-Clause
manifest.json                      package metadata
SHA256SUMS                         every shipped byte, pinned
```

## Build and test

```bash
make test       # unit tests for entry table and image parser
make asan       # ASan/UBSan
make all        # build lib/libezhle.a
```

## Install

```bash
./install.sh
# or
PREFIX="$HOME/.local" ./install.sh
```

The installer verifies `SHA256SUMS` before copying. It installs to
`$PREFIX/lib/ezhle/fds`. No network access, no sudo.

To verify by hand:

```bash
shasum -a 256 -c SHA256SUMS     # macOS
sha256sum -c SHA256SUMS         # Linux
```

## Use

```c
#include "ezhle/ezhle.h"
#include "ezhle/ezhle_fds.h"
#include "ezhle/ezhle_fds_image.h"

ezhle_fds_image *disk = ezhle_fds_image_open(bytes, len, &err);
const ezhle_fds_image_file *file = ezhle_fds_image_find(disk, 0);
/* attach the HLE to a core you own, then dispatch a guest JSR */
ezhle_fds_call(core, &regs, 0xE97D);
```

The library never dereferences a guest pointer directly; every access goes
through the `ezhle_bus_t` callbacks the host core supplies.

## Coverage (honest)

| Area | State |
|---|---|
| Attach / reset / destroy | done |
| Reset vector `$FFFC/$FFFD` | done |
| Entry table | 36 documented addresses |
| Implemented services | `Delay131` `$E149`, `Pixel2NamConv` `$E97D`, `Nam2PixelConv` `$E997` |
| Disk image parser | fwNES header and headerless, multi-side, file table, bounds |
| Everything else | `EZHLE_ERR_UNIMPL`, logged, no silent success |
| Playable game | **no** — this package is not a full NES/FDS core |

## Provenance

Clean-room from public documentation only: NESdev wiki FDS BIOS, FDS file
format, and FDS disk format pages. No disassembly, no retail BIOS, no game
data, no GPL source. See `NOTICE`.

## License

BSD-2-Clause. See `LICENSE`.
