# CSV Data Formats

All state lives in `data/` next to the binary. The folder is created
automatically on save (exit, or the manager's *Save data to disk*), and
every file is plain UTF-8 CSV with a header line, reloaded on the next
start. Deleting `data/` resets the app to the demo seed.

## users.csv

```
username,salt,hash,role,blocked,failedLogins,balance,loyaltyPoints,lifetimePoints,completedRentals
```

| Column | Meaning |
|---|---|
| `username` | unique login name; also the hash-table key |
| `salt` | random 8-byte salt from the OS entropy pool, hex-encoded (16 chars) |
| `hash` | SHA-256(salt + password), hex-encoded |
| `role` | `Customer` / `Staff` / `Manager` |
| `blocked` | `1` after 3 failed logins (or manual block), else `0` |
| `failedLogins` | consecutive failed-login counter, reset on success |
| `balance` | outstanding amount for customers, `0` for Staff/Manager |
| `loyaltyPoints` | points available for redemption (customers, since v2.7.0) |
| `lifetimePoints` | points earned over all time — never spent, decides the tier |
| `completedRentals` | returns processed for this customer |

The three loyalty columns exist since v2.7.0. Rows from older files
(without them) still load and default to all-zero — the tier is derived
from the lifetime points on every read, never stored.

The plaintext password is never stored. Authentication recomputes
SHA-256(salt + attempt) and compares hashes — see `SHA256.{h,cpp}`,
verified against the official NIST test vectors.

## cars.csv

```
name,brand,pricePerDay,status,income,maintenanceCost
```

| Column | Meaning |
|---|---|
| `name` | unique car name; also the AVL-tree key |
| `brand` | free text (e.g. `Ikco`, `Mercedes`) |
| `pricePerDay` | daily rate, decimal |
| `status` | integer — see table below |
| `income` | cumulative gross income, feeds the income report |
| `maintenanceCost` | cumulative maintenance cost, feeds the income report |

| `status` | Meaning |
|---|---|
| `0` | Available |
| `1` | Reserved |
| `2` | Rented |
| `3` | Maintenance |

Row order equals the fleet's insertion order (the doubly linked list)
and is preserved across runs.

## reservations.csv

```
id,carName,customer,requestTime,startDate,endDate
```

| Column | Meaning |
|---|---|
| `id` | unique reservation id |
| `carName` | car name, matches `cars.csv` |
| `customer` | username, matches `users.csv` |
| `requestTime` | booking creation order (monotonic counter, not a date) |
| `startDate` / `endDate` | rental window as ISO dates, `YYYY-MM-DD` (inclusive). Files written before v2.3.0 used bare day numbers — both load. |

Within a car, the booking queue serves the earliest `startDate` first
(binary min-heap); ties are broken by `requestTime` (FIFO). Live
reservations survive restarts because the queues are rebuilt from this
file on load.

## waitlist.csv (data/)   — since v2.6.0

The customer wishes that could not be booked directly. One row per
wish, dumped in fleet insertion order (heap order within a car):

```
carName,customer,startDate,endDate,joinOrder
Pride 131,ali,2026-12-01,2026-12-02,1
```

| Column | Meaning |
|---|---|
| `carName` | car name, matches `cars.csv` |
| `customer` | username, matches `users.csv` |
| `startDate` / `endDate` | desired rental window, ISO `YYYY-MM-DD` (inclusive) |
| `joinOrder` | position in the global first-come counter |

Loading rebuilds each car's wish heap and continues the global join
counter at `max(joinOrder) + 1`. Wishes are served by earliest desired
pickup first, `joinOrder` as the tie-break — mirroring
`reservations.csv`. Rows referencing a deleted car, an unknown user or
a non-customer role are dropped on load instead of crashing.

## Exported reports (reports/)

Separate from `data/`, the manager can export two snapshot reports
from the *Export CSV reports* menu entry. They are written on demand
into `reports/` (created automatically, git-ignored) and are **not**
reloaded on startup — the app state always lives in `data/`.

### fleet_report.csv

One row per car, **sorted by name** (the AVL in-order walk), so two
exports of the same state are byte-identical:

```
name,brand,pricePerDay,status,income,maintenanceCost,netRevenue
Benz C200,Mercedes,180,RESERVED,0,0,0
```

| Column | Meaning |
|---|---|
| `name` / `brand` | as in `cars.csv` |
| `pricePerDay` | daily rate |
| `status` | `AVAILABLE` / `RESERVED` / `RENTED` / `MAINTENANCE` |
| `income` | cumulative gross income (the ledger) |
| `maintenanceCost` | cumulative maintenance spend |
| `netRevenue` | `income - maintenanceCost` |

### bookings_report.csv

One row per **live** reservation across the fleet, ordered by start
date (reservation id breaks ties):

```
id,carName,customer,startDate,endDate,durationDays
2,Samand LX,ali,2026-11-03,2026-11-05,3
```

| Column | Meaning |
|---|---|
| `id` | reservation id (equals creation order) |
| `carName` / `customer` | references into `cars.csv` / `users.csv` |
| `startDate` / `endDate` | rental window as ISO dates, `YYYY-MM-DD` (inclusive) |
| `durationDays` | `endDate - startDate + 1` |

Completed (returned) bookings are not listed — the fleet report's
`income` column is where their revenue lives.

## reports/report.json (JSON)

Written by manager option **8** next to the two CSV sheets — the same
state as one JSON object:

```json
{
  "report": "car-rental-system",
  "generatedAt": "2026-11-05",
  "stats":    { "totalCars": 8, "grossIncome": 0, "...": "..." },
  "fleet":    [ { "name": "Benz C200", "brand": "Mercedes",
                  "pricePerDay": 180, "status": "AVAILABLE",
                  "income": 0, "maintenanceCost": 0, "netRevenue": 0,
                  "maintenanceLog": [] } ],
  "bookings": [ { "id": 1, "carName": "Benz C200", "customer": "ali",
                  "startDate": "2026-11-10", "endDate": "2026-11-12",
                  "durationDays": 3 } ]
}
```

- `stats` mirrors the manager's statistics screen one-to-one.
- `fleet` is name-sorted (same order as `fleet_report.csv`);
  `maintenanceLog` carries `{date, description, cost}` records.
- `bookings` is ordered by pickup date (id breaks ties), same as
  `bookings_report.csv`.
- Every string is escaped per RFC 8259 — the unit suite parses the
  exported file with its own strict JSON parser, so the format is a
  tested contract, not a best effort.

## audit_log.csv (data/)

Append-only trail of privileged actions, written by the facade on
every save. One record per line; fields containing commas or quotes
are RFC 4180 quoted, newlines inside details are flattened to the
two-character sequence `\n` (and `\r`), a literal backslash is
doubled — all unescaped on load.

```
date,actor,action,result,detail
2026-11-08,?,car.added,ok,Benz C200
2026-11-08,admin,login.ok,ok,Manager
2026-11-08,sara,export.fleet,denied,"denied: needs export-reports"
2026-11-08,admin,car.returned,ok,Benz C200 (late penalty applied)
```

- `date` — ISO calendar date of the entry
- `actor` — username; `?` when no session was active
- `action` — dotted id: `login.ok`, `login.failed`, `login.denied`,
  `account.autoblock`, `customer.registered`, `user.created`,
  `user.blocked`, `user.unblocked`, `car.added`, `car.delivered`,
  `car.returned`, `reservation.made`, `maintenance.sent`,
  `maintenance.finished`, `maintenance.logged`, `export.fleet`,
  `export.bookings`, `waitlist.joined`, `waitlist.left`,
  `waitlist.served`, `waitlist.promoted`
- `result` — `ok` or `denied` (failed logins count as denied)
- `detail` — full free-text context (the on-screen table truncates
  it to 30 characters; the file never does)

## Notes

- Files are rewritten in full on every save; do not edit them while the
  app is running.
- Money columns use plain decimal notation, two digits typical.
- The layout is intentionally boring on purpose: any spreadsheet can
  open these files, and `docs/DATA_FORMATS.md` is the contract.
