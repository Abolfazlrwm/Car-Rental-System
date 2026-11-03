# Changelog

All notable changes to the Car Rental System are documented in this file.
The detailed v1.0 → v2.0 story lives in
[docs/IMPROVEMENTS.md](docs/IMPROVEMENTS.md).

## [2.1.0] — 2026-11-03

### Added

- **Fleet queries on `RentalManager`** — `findBookableCars()`,
  `findCarsInPriceRange(min, max)` (inclusive bounds, sorted by daily
  rate, name as tie-breaker) and a `forEachCarByPrice()` sorted view;
  covered by 47 new regression tests (suite now at 2,164 checks)
- **Usage guide** (`docs/USAGE.md`) — every role's menu explained step
  by step, with typical sessions
- **CSV data reference** (`docs/DATA_FORMATS.md`) — the exact contract
  of `data/users.csv`, `cars.csv` and `reservations.csv`
- **`--version` / `--help` flags** — non-interactive queries exit
  before touching `data/`
- **CI sanitizer job** — the full suite runs under AddressSanitizer +
  UBSan on every push
- `make asan` / `make format` targets, plus a matching `.clang-format`
- Community files: issue templates (bug / feature), PR template,
  [Security policy](SECURITY.md), [Code of Conduct](CODE_OF_CONDUCT.md)
  and this changelog

### Changed

- CMake project version now tracks `APP_VERSION` in `mainApp.cpp`
  (both report 2.1.0)

## [2.0.0] — 2026-10-20

Portfolio-grade overhaul while staying dependency-free:

- `basic_structures/` collapsed into header-only generic templates
  (AVL tree, hash table with auto-rehash, priority queue, linked list)
- Salted SHA-256 password hashing (FIPS 180-4), account lockout,
  overlapping-reservation rejection, per-car income ledger
- `RentalManager` facade with a strict ownership model, CSV
  persistence for users / cars / reservations
- 2,117-check dependency-free test suite, Makefile + CMake/CTest,
  GitHub Actions CI (GCC + Clang), rewritten README, MIT license

## [1.0.0] — 2026-02-23

The original data-structures course project: doubly linked list,
AVL tree, hash table, binary-heap priority queue and the first
version of the console rental app.
