# Handoff — FDS-104

## Files changed
- `docs/FDS-COVERAGE.md` (new) — 36-row coverage matrix + "What a game needs next".
- `handoff.md` (new) — this file.

## Commands and exit codes
- `make test` (before): exit 0 (all fds + image tests PASS).
- `make test` (after): exit 0 (all fds + image tests PASS).
- `git add docs/FDS-COVERAGE.md handoff.md && git commit ...`: pending.

## Open questions
- Four entries (0xE19D VINTWaitHandler, 0xE1CE DiskTransfer, 0xE1D9 DiskSkipBytes, 0xE1EF AcknowledgeDelay) are interrupt/vector-control targets rather than documented JSR services; classified `unknown` and left UNIMPL. If maintainers consider the vector-control section sufficient contract, they may be reclassified `unimpl`.
- No code or tests were changed; tree behaviour unchanged.
