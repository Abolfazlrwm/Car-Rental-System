# Changelog

All notable changes to the Car Rental System are documented in this file.
The detailed v1.0 → v2.0 story lives in
[docs/IMPROVEMENTS.md](docs/IMPROVEMENTS.md).

## [2.2.0] — 2026-11-04

### Added

- **Keyword fleet search** — `RentalManager::searchFleet()` matches
  car names **and** brands case-insensitively (insertion order kept,
  empty keyword matches nothing); wired into the manager menu
- **Fleet statistics snapshot** — `FleetStats` + `computeStats()`:
  fleet composition by status, active bookings, gross/NET revenue,
  utilization % and the top-earning car, in one pass; rendered by the
  new *Fleet statistics* screen
- **CSV report export** — `exportFleetReport()` (name-sorted fleet
  sheet) and `exportBookingReport()` (live bookings by start date)
  write `reports/*.csv`; the layout is documented in
  [docs/DATA_FORMATS.md](docs/DATA_FORMATS.md)
- 72 new regression tests covering the keyword search, the statistics
  lifecycle (booking → delivery → late return → maintenance) and the
  report round-trips — suite now at **2,236 checks**

### Fixed

- `scripts/demo.sh` attached its input heredoc to `sed` instead of the
  app, so the advertised walkthrough never reached `car_rental` and
  died with SIGPIPE (exit 141) before printing anything

### Changed

- Manager menu renumbered to 13 entries — search (4), statistics (5),
  income report (6), export reports (7), account tools shifted down

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
