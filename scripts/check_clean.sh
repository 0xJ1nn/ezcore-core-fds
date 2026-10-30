#!/bin/sh
# Clean-room and release hygiene check for the FDS HLE package.
# Runs on tracked files only. Exits non-zero on any finding.
set -eu

cd "$(git rev-parse --show-toplevel)"
fail=0
list=$(mktemp)
trap 'rm -f "$list"' EXIT
git ls-files > "$list"

# 1. No firmware/ROM/disk/key material, even as a fixture.
bad_ext=$(grep -Ei '\.(nes|fds|rom|bin|iso|img|cue|chd|sfc|z64|n64|gb|gbc|gba|nds|cso|pbp|elf|prx|key|der|pem|nand|sav)$' "$list" || true)
if [ -n "$bad_ext" ]; then
    echo "FAIL: forbidden file type:" >&2
    echo "$bad_ext" >&2
    fail=1
fi

# 2. The only tracked binary is the static library we build from source.
while IFS= read -r f; do
    [ -f "$f" ] || continue
    if [ "$f" = "lib/libezhle.a" ]; then
        continue
    fi
    if file --brief --mime "$f" | grep -q 'charset=binary'; then
        echo "FAIL: unexpected binary: $f" >&2
        fail=1
    fi
done < "$list"

# 3. No secrets, absolute home paths, internal orchestration, or release markers.
# The scanner itself is excluded because it contains the patterns.
while IFS= read -r f; do
    case "$f" in
        scripts/check_clean.sh|lib/*.a) continue ;;
    esac
    [ -f "$f" ] || continue
    if grep -Iq . "$f" 2>/dev/null; then
        if grep -nE '/Users/|/home/|sk-[A-Za-z0-9_-]{20,}|ghp_[A-Za-z0-9]{20,}|github_pat_|BEGIN (RSA|OPENSSH|EC) PRIVATE KEY|delegate_task|CONFIDENTIAL|internal use only' "$f"; then
            echo "FAIL: forbidden token in $f" >&2
            fail=1
        fi
    fi
done < "$list"

# 4. License and provenance must be present.
grep -q 'BSD 2-Clause License' LICENSE || { echo "FAIL: LICENSE is not BSD-2" >&2; fail=1; }
[ -f docs/SOURCES.md ] || { echo "FAIL: docs/SOURCES.md missing" >&2; fail=1; }
[ -f docs/CLEAN_ROOM.md ] || { echo "FAIL: docs/CLEAN_ROOM.md missing" >&2; fail=1; }

if [ "$fail" -eq 0 ]; then
    echo "clean-room check: PASS ($(wc -l < "$list" | tr -d ' ') tracked files)"
fi
exit "$fail"
