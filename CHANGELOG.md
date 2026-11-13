# Changelog

All notable changes to the Car Rental System are documented in this file.
The detailed v1.0 → v2.0 story lives in
[docs/IMPROVEMENTS.md](docs/IMPROVEMENTS.md).

## [3.1.0] — 2026-11-13

### Added

- **Pickup reminders** — the customer hears about everything due
  this week without asking. Signing in and opening "My reservations"
  both print a `PICKUP REMINDERS | next 7 days` block when pickups
  are due: soonest first, one line per booking — its id, the car,
  the pickup day and how far out it sits (`today`, `tomorrow`,
  `in N days`), with the `[series #id]` tag on series occurrences.
  The rules live in the pure layer (`Recurring.h`, same layering as
  the plan maths): a pickup reminds from the moment it is bookable
  until it is served — `today` still reminds (the car is due back
  before it goes out again), the past never does, and the booking
  that is OUT right now never reminds — it was already handed over;
  its later series occurrences do. Cancelling a series clears its
  reminders in one step, and non-customers hold no reminders at
  all. No file format changes — reminders are a read-only view
  over the bookings that already exist.

### Tests

- Suite grown from 5,179 to **5,258 checks** (+79) across two new
  suites: "Recurring series: reminder maths" (horizon filter, desk
  phrasing, ordering) and "RentalManager: pickup reminders" (a
  pickup due today, series tags riding along, hand-overs silencing
  the screen, returns, one-step cancellation, and the far one-off
  only a wide horizon can see). Still zero dependencies, still
  leak-free under ASan + UBSan.

## [3.0.0] — 2026-11-11

### Added

- **Recurring reservations** — when booking, choose a plan
  (weekly / biweekly / monthly) and how many times (2–8): the same
  car and duration are booked again one plan step later, as many
  times as requested. The whole series is created ATOMICALLY — a
  mid-series clash with an existing booking leaves nothing behind
  and names the offending occurrence. Monthly pickups re-derive
  from the first start with day-of-month clamping (Jan 31 lands on
  Feb 28, Mar 31, Apr 30…). Every occurrence carries the shared
  series tag (a new column in `reservations.csv`, loaded tolerantly
  from pre-3.0.0 files); "My reservations" lists each occurrence
  with its `[series #id]` and cancels the still-queued ones in one
  step — the occurrence already handed over stays, because you
  cannot un-drive a car. Audited as `recurring.created` /
  `recurring.cancelled`.
- **Dynamic discounts** — invoices are priced by one pure engine
  (`Pricing.h`): a LONG-RENTAL rate from the booked days (3+ days
  3%, 7+ days 7%, 14+ days 12%, 30+ days 18%) and a RANK rate from
  the tier held at return (Silver 3%, Gold 6%, Platinum 10%),
  stacked multiplicatively so discounts never compound past 100%.
  Late days stay at the raw daily rate — a discount never softens a
  penalty. A confirmed booking prints the estimate from the very
  same engine; a discounted return announces the amount at the desk
  and lands in the trail as `discount.applied`; loyalty points
  accrue on the invoiced (discounted) total.
- **Rental history + monthly report** — every processed return now
  enters an append-only ledger, `data/rental_history.csv`, with the
  full invoice breakdown. The manager menu grows option 17 (after
  exit, so no scripted flow re-numbers): rentals completed, revenue
  booked with the discounts-given and late-fee split, distinct
  customers, revenue by car as ASCII bars, returns per day and the
  busiest day — plus the same numbers as
  `reports/monthly_YYYY-MM.csv`. The screen is gated by
  `view-analytics`, the export by `export-reports`, both audited.
  The demo walkthrough now ends with the manager reading the
  current month.

### Changed

- **Invoice totals carry discounts now** — every pinned invoice in
  the suite was re-derived from the rate tables (a 5-day Benz
  booking bills 846.81 instead of 900.00 for a seeded Silver
  member); the parisa loyalty story books a 60-day rental so it
  still crosses into Silver mid-history.

### Fixed

- `Car::dropReservation()` freed a booking BEFORE draining its
  heap, so `extractMin()` re-heapified through freed memory — a
  latent use-after-free surfaced by the series-cancellation flow
  under AddressSanitizer, fixed by draining first and freeing last.

### Tests

- Suite grown from 4,896 to **5,179 checks** (+283) across seven new
  suites: recurring plan maths, recurring series end to end,
  discount maths, discounted invoices, the history ledger and its
  CSV round-trip, the monthly builder, and the monthly report flow
  (gating, byte-checked CSV export, persistence). Still zero
  dependencies, still leak-free under ASan + UBSan.

## [2.9.0] — 2026-11-09

### Added

- **Loyalty CSV report** — the export option now also writes
  `reports/loyalty_report.csv`: header + one username-sorted row per
  customer (derived rank, spendable vs lifetime points, completed
  rentals, points still missing for the next rank). Two exports of
  the same state are byte-identical — the same diff-friendly rule
  the fleet sheet and the JSON member array follow. Gated by
  `export-reports` like every other report file.
- **Rank-up notifications** — a return that crosses a rank threshold
  is announced at the desk ("Rank up! parisa crossed into Silver")
  and lands in the audit trail as `loyalty.rankup`
  (`parisa Bronze -> Silver`). The manager's program overview grew a
  "Recent rank-ups" section listing the five newest promotions,
  straight from the trail.

### Tests

- Suite grown from 4,877 to **4,896 checks**: rank-up detection is
  pinned on both sides (Bronze → Silver announced and audited with an
  ISO date; Silver → Silver and Bronze → Bronze silent, the notice
  cleared), the CSV report is byte-compared in three shapes
  (populated, header-only for an empty program, denied for staff
  with the refusal audited) — still zero dependencies, still
  leak-free under ASan + UBSan.

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
