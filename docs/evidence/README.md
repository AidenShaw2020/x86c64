# Recorded validation

These are recorded results from the development machine, not CI runs and
not physical-hardware validation. JSON files retain relative artifact paths
and hashes so test builds can be distinguished from the production package.
Large generated artifacts and VICE snapshots are not stored in Git.

- `test-report.json`: Python image tests, 311,751 native assertions and
  VICE success/error smoke cases.
- `cpu-check-report.json`: 3,072 instruction vectors checked on the C64 build.
- `1581-check-report.json`: true-drive IEC sector read/write and persistent
  host-image verification, including the directory-track boundary.
- `glabios-1581-test-report.json`: cartridge reset, POST, DOS commands,
  file creation/readback and persistence after VICE exit (766 seconds).
- `1581-resume-report.json`: fresh ECHO after restoring the actual booted machine.
- `1581-file-read-report.json`: fresh TYPE of the persisted DOS file, with
  additional reads and actual VIC-II pixel verification.
- `production-release-report.json`: fresh cold boot of the exact production
  CRT/D81 and a new DOS response, recorded by `tools/verify_release.py`.

The production test uses disposable copies. Screenshots in `../screenshots`
come from that real run. The automatic persistence test uses a distinct
cartridge with `FD_AUTOTEST`; its hash intentionally differs from production.
