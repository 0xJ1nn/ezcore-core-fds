#!/usr/bin/env python3
"""Generate synthetic FDS image corpus for the fuzz harness. No ROM/BIOS."""
from pathlib import Path
import sys

out = Path(sys.argv[1] if len(sys.argv) > 1 else "corpus/fuzz_fds_image")
out.mkdir(parents=True, exist_ok=True)

s = bytearray(65500)
s[0] = 0x01
s[1:15] = b"*NINTENDO-HVC*"
s[0x38] = 0x02
s[0x39] = 0x00

def w(name: str, data: bytes) -> None:
    (out / name).write_bytes(data)

w("headerless_empty", bytes(s[:0x3A]))

h = bytearray([0x46, 0x44, 0x53, 0x1A, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]) + s
w("fwnes_header_empty", bytes(h[:0x10 + 0x3A]))

w("truncated", bytes([0x01, 0x2A] * 10))

s2 = bytearray(s)
s2[1] = 0x58
w("bad_magic", bytes(s2[:0x50]))

h2 = bytearray([0x46, 0x44, 0x53, 0x1A, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]) + s + s
w("multi_side_header", bytes(h2[:0x10 + 0x3A]))

print("corpus:", len([p for p in out.iterdir() if p.is_file()]))
