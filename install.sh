#!/usr/bin/env bash
# Install the FDS HLE library package. Verifies every pinned byte first.
set -euo pipefail
cd "$(dirname "$0")"

PREFIX="${PREFIX:-$HOME/.local}"
DEST="$PREFIX/lib/ezhle/fds"

if command -v sha256sum >/dev/null 2>&1; then
  sha256sum -c SHA256SUMS
elif command -v shasum >/dev/null 2>&1; then
  shasum -a 256 -c SHA256SUMS
else
  echo "install.sh: need sha256sum or shasum to verify the package" >&2
  exit 1
fi

mkdir -p "$DEST"
cp -R include lib manifest.json LICENSE NOTICE "$DEST"/
echo "installed FDS HLE $(python3 -c 'import json;print(json.load(open("manifest.json"))["version"])' 2>/dev/null || echo 0.1.0) -> $DEST"
