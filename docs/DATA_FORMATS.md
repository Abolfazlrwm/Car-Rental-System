# CSV Data Formats

All state lives in `data/` next to the binary. The folder is created
automatically on save (exit, or the manager's *Save data to disk*), and
every file is plain UTF-8 CSV with a header line, reloaded on the next
start. Deleting `data/` resets the app to the demo seed.

## users.csv

```
username,salt,hash,role,blocked,failedLogins,balance
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
| `requestTime` | day number on which the booking was made |
| `startDate` / `endDate` | rental window as day numbers (inclusive) |

Within a car, the booking queue serves the earliest `startDate` first
(binary min-heap); ties are broken by `requestTime` (FIFO). Live
reservations survive restarts because the queues are rebuilt from this
file on load.

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
2,Samand LX,ali,2,4,3
```

| Column | Meaning |
|---|---|
| `id` | reservation id (equals creation order) |
| `carName` / `customer` | references into `cars.csv` / `users.csv` |
| `startDate` / `endDate` | rental window as day numbers (inclusive) |
| `durationDays` | `endDate - startDate + 1` |

Completed (returned) bookings are not listed — the fleet report's
`income` column is where their revenue lives.

## Notes

- Files are rewritten in full on every save; do not edit them while the
  app is running.
- Money columns use plain decimal notation, two digits typical.
- The layout is intentionally boring on purpose: any spreadsheet can
  open these files, and `docs/DATA_FORMATS.md` is the contract.
