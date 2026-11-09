# Changelog

All notable changes to the Car Rental System are documented in this file.
The detailed v1.0 → v2.0 story lives in
[docs/IMPROVEMENTS.md](docs/IMPROVEMENTS.md).

## [2.8.0] — 2026-11-09

### Added

- **Loyalty standings in the JSON report** — `reports/report.json`
  now carries a `loyalty` section next to `stats`, `fleet` and
  `bookings`: the program snapshot (members per rank, outstanding
  points, their bill-credit liability, the deterministic top member)
  plus one row per customer — username, derived rank, spendable and
  lifetime points, completed rentals and the points still missing
  for the next rank. Member rows are username-sorted, so the array
  is stable regardless of hash-table walk order.
- The section is built by the new facade method
  `computeLoyaltyOverview()` — the very computation the manager's
  program overview renders — so the file and the screen can never
  drift apart, the same one-computation principle the stats block
  has followed since v2.4.0. The interactive screen stays gated by
  `view-analytics`; the export is permission-checked upstream by the
  manager menu that offers it.

### Tests

- Suite grown from 4,804 to **4,877 checks** (a dedicated JSON suite
  drives points through the REAL accrual flow — reserve → deliver →
  return — and pins the earned standing inside the file; the empty
  and seeded-system export tests now also verify the loyalty
  section, all-zero for a fresh program and equal to `ali`'s seeded
  Silver standing for the demo fleet) — still zero dependencies,
  still leak-free under ASan + UBSan.

## [2.7.0] — 2026-11-09

### Added

- **Loyalty program with tiers** — every completed rental (return
  processed, invoice written) pays points on the invoice total:
  1 point per 10.00 spent, multiplied by the rank the customer
  already holds — Bronze ×1.0, Silver ×1.25, Gold ×1.5, Platinum
  ×2.0. Ranks live on **lifetime points** (never decremented) and
  are derived on every read, so a customer crosses into the next
  rank the moment the threshold is met and there is no stored tier
  to forget during a save/load round-trip.
- **Points redemption against the bill** — 10 points = 1.00 credit,
  minimum 50 points, whole currency only (steps of 10), and never
  more than the outstanding bill: points can be spent, not cashed
  out, so nothing is ever wasted. Both moments land in the audit
  trail as `loyalty.earned` (with the rank the customer lands on)
  and `loyalty.redeemed`.
- **Loyalty card screen** (customer option 10) — rank, a progress
  bar to the next rank, spendable vs lifetime points, completed
  rentals and the program's earn/redeem rules in one view.
- **Redeem screen** (customer option 11) — walks through a
  redemption with balance and points in view; a cleared bill is
  pointed out up front.
- **Loyalty program overview** (manager option 8) — members per
  rank as bars, outstanding points, their credit liability and a
  deterministic top member (points desc, name as tie-break);
  gated by the `view-analytics` permission like the other
  dashboards, denials audited.
- Customer menu grown to 12 entries, manager menu to 16.

### Changed

- `users.csv` customer rows carry three loyalty counters
  (`loyaltyPoints,lifetimePoints,completedRentals`); pre-2.7.0
  seven-column files still load with an all-zero standing.
- The demo customer `ali` seeds as a Silver member holding 340
  points over 3 completed rentals, so the loyalty screens have
  something to show on a fresh clone.

### Tests

- Suite grown from 4,668 to **4,804 checks** (three new suites:
  the tier/accrual/redemption maths, the full facade flow through
  three completed rentals with a mid-history rank-up and every
  redemption refusal path, and the manager overview built from a
  hand-written `users.csv` with a legacy seven-column row and a
  top-member tie) — still zero dependencies, still leak-free
  under ASan + UBSan.

## [2.6.0] — 2026-11-09

### Added

- **Waitlist with auto-booking** — a car whose dates are taken no
  longer turns customers away: they file a wish (car + desired
  range) into the car's own `PriorityQueue`, ranked earliest desired
  pickup first with a global first-come counter as the tie-break —
  the exact priority shape of the booking schedule. The moment a
  return frees the window (or maintenance finishes), the facade
  promotes the best-fitting wish into a real reservation and logs it
  as `waitlist.promoted`; clashing wishes stay queued and the front
  desk can serve the queue by hand (`handle-deliveries` permission,
  denials audited). Wishes round-trip through the new
  `data/waitlist.csv` together with the global join counter.
- **Date-range availability search** — the customer menu answers
  "which cars are free 2026-12-01 → 2026-12-03?" in one screen:
  bookable status AND no queued booking overlapping the range, in
  fleet insertion order.
- **Waitlist depth on the analytics dashboard** — a "Waitlist depth
  per car" bar section (deepest queue first) plus a fleet-wide
  waiting counter next to the overdue-returns warning.
- Customer menu grown to 10 entries (find-free-cars, join waitlist,
  my-waitlist/leave) with a waitlist tip after every overlap-rejected
  reservation; staff menu gains "Serve waitlist now".

### Changed

- Returns and finished maintenance may now trigger a promotion right
  after their own audit entry, so a freed window never sits idle
  while someone is waiting for it.
- `joinWaitlist` refuses (with a pointer to the reservation flow)
  when the requested range is directly bookable — queueing people
  who do not need to wait would be a lie.

### Tests

- Suite grown from 4,542 to **4,668 checks** (three new suites: wish
  priority rules, the car-level wish heap, and the facade flow —
  every rejection path, both auto-promotion triggers, the
  clashing-wish survival case and the CSV round-trip) — still zero
  dependencies, still leak-free under ASan + UBSan.

## [2.5.0] — 2026-11-08

### Added

- **Fine-grained permissions** — an explicit role matrix
  (`manager_system/Permissions.h`) re-checked by the facade on every
  manager-exclusive call: managers hold all seven permissions, staff
  keep the front-desk trio (deliver, return, maintenance), customers
  none. Denials land in the audit trail with the missing permission
  named; headless calls with no active session (scripts, tests,
  demo seeding) keep working.
- **Audit trail** — the facade records logins (success, wrong
  password, unknown user, blocked account, the 3-strike autoblock),
  account and staff creation, fleet changes, maintenance
  transitions, deliveries, returns (late penalties flagged),
  reservations and report exports in an append-only trail built on
  the project's own `DoublyLinkedList`, round-tripped through
  `data/audit_log.csv` with RFC 4180 quoting and newline
  flattening.
- **Audit log screen** — manager menu option 7 lists the newest 15
  entries (date, actor, action, result, detail), denials in red.
- **Dev container** — `.devcontainer/` builds an Ubuntu 24.04 image
  with build-essential, CMake, gdb and clang-format and runs `make`
  on creation: clone, open, compile.

### Changed

- Account management (block / unblock, staff creation) and report
  export are enforced manager-only at the facade level — until now
  only the CLI menus separated the roles.
- Manager menu renumbered: audit log is option 7, everything after
  the analytics dashboard shifts by one.

### Fixed

- `make` (and `make test / bench / asan`) failed with *missing
  separator* since the 2.4.0 build edit had swapped the recipe tabs
  for spaces — CI never noticed because it builds via CMake.

### Tests

- Suite grown from 4,457 to **4,542 checks** (three new suites: the
  role matrix, the audit-log CSV round-trip and RBAC enforcement)
  — still zero dependencies, still leak-free under ASan + UBSan.

## [2.4.0] — 2026-11-05

### Added

- **JSON report export** — the export option now also writes
  `reports/report.json`, one self-contained file holding the stats
  snapshot, the name-sorted fleet (maintenance logs included) and
  every live booking ordered by pickup date, dates as ISO strings.
  Hand-rolled RFC 8259 escaping (quotes, backslashes, control bytes
  as `\u00XX`, multi-byte UTF-8 passed through) — and the suite
  validates the produced file with its own strict recursive-descent
  JSON parser, so the output is proven parseable, not just eyeballed
- **Fleet analytics dashboard** (manager menu → 6) — live ASCII bar
  charts: fleet composition by status, the fleet split over
  daily-rate buckets, bookings per car, the next five pickups with
  "in N days" countdowns, and a warning line listing bookings past
  their end date. The maths lives in the new
  `manager_system/Analytics` module — pure (today's date is a
  parameter), deterministic and fully unit-tested; the UI only draws
- `Date::todaySerial()` — the calendar layer can now read the system
  clock, powering the report's `generatedAt` stamp and the
  dashboard's countdowns
- 119 new checks: JSON escaping edge cases (quotes, backslashes,
  control bytes, DEL and UTF-8 pass-through), full-file parse
  round-trips, hostile car names (`P\206 "GT"`, a raw newline —
  neither of which CSV could ever carry), exact bar-rendering maths,
  and deterministic analytics against a synthetic clock — suite now
  at **4,457 checks**, still leak-free under AddressSanitizer + UBSan

### Changed

- Export option renamed to "Export CSV & JSON reports"; the CSV pair
  is byte-identical to 2.3.0's
- Manager menu renumbered (new option 6); README, USAGE and the
  landing page updated to match

## [2.3.0] — 2026-11-05

### Added

- **Real calendar dates** — the whole rental flow now speaks
  `YYYY-MM-DD`: reservations, returns, maintenance logs and every CSV
  file. Strict parsing on the proleptic Gregorian calendar with full
  leap-year handling (`2024-02-29` parses, `1900-02-29` and
  `2026-02-30` are rejected) via the new zero-dependency
  `manager_system/Date.{h,cpp}`. The scheduler keeps exact serial-day
  arithmetic internally, and pre-2.3.0 data files (bare day numbers)
  still load — documented in [docs/DATA_FORMATS.md](docs/DATA_FORMATS.md)
- **Container benchmark suite** (`make bench` / `benchmarks/`) — the
  hand-written AVL tree, hash table, binary heap and linked list race
  `std::map`, `std::unordered_map`, `std::priority_queue` and
  `std::list` under the same access patterns the app produces;
  measured numbers and an honest read of them live in
  [docs/BENCHMARKS.md](docs/BENCHMARKS.md)
- **GitHub Pages landing site** — `docs-site/` plus a
  `docs.yml` deploy workflow;
  https://abolfazlrwm.github.io/Car-Rental-System/
- 2,102 new checks: strict/invalid ISO parsing, century leap-year
  rules, ISO↔serial round-trips (every single day of 1970–1971 plus a
  sweep to 2100), chronological ordering of ISO strings, and
  legacy-CSV / corrupt-row loading — suite now at **4,338 checks**,
  still leak-free under AddressSanitizer + UBSan

### Changed

- `reservations.csv` and the booking report store `startDate` /
  `endDate` as ISO dates; old day-number files remain loadable
- Maintenance log dates are validated `YYYY-MM-DD` in the UI
- `make bench` target and a `bench_containers` CMake target (built,
  never run by CI)

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
