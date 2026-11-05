# Usage Guide

Everything you need to drive the console app. Build first (see the
[README](../README.md) for all build options):

```bash
make            # builds car_rental + unit_tests
./car_rental    # start the app
```

On first start the app seeds a demo fleet (8 cars) and three accounts,
then saves everything to `data/` and reloads it on every later run.

## Command-line flags

| Flag | Effect |
|---|---|
| `--version`, `-v` | print version and exit |
| `--help`, `-h` | show usage help and demo accounts |

## Demo accounts

| Role | Username | Password | Can do |
|---|---|---|---|
| Manager | `admin` | `manager123` | everything + user & fleet administration |
| Staff | `sara` | `staff123` | fleet desk: handover, returns, maintenance |
| Customer | `ali` | `customer123` | browse, reserve, renew, pay |

Passwords are stored as salted SHA-256 hashes — nobody (including the
manager) can read them back. Three consecutive wrong passwords block an
account until a manager unblocks it.

## Main menu

```
===== MAIN MENU =====
  1. Register (customer)
  2. Login
  3. Exit
```

Self-registration always creates a **Customer** account. Staff and
Manager accounts can only be created by a logged-in Manager.

## Dates are real calendar dates

Since v2.3.0 the app speaks **ISO calendar dates** (`YYYY-MM-DD`):
reservation windows, maintenance logs and the CSV exports are all
date-based, with strict validation (leap years included — `2024-02-29`
works, `2026-02-30` is re-asked). Internally the scheduler still uses
serial day numbers (days since 1970-01-01), so all the arithmetic —
duration, overlap, late penalties — stays exact. Data files saved by
older versions (bare day numbers) still load.

## Customer menu

| Option | What it does |
|---|---|
| 1. View fleet | table of all cars: name, brand, price/day, status |
| 2. Search car by name | O(log n) lookup in the AVL index |
| 3. My reservations | your bookings and their status |
| 4. Make a reservation | pick a car + start/end date; overlap checks apply |
| 5. Renew a rental | extend your current rental if nobody is waiting |
| 6. Pay my bill | settle what you owe (rental + late penalties) |
| 7. Logout | back to the main menu |

A typical session:

```text
===== CUSTOMER MENU | ali | balance due: 0.00 =====
  Choice: 4
  Car name: Benz C200
  Start date (YYYY-MM-DD): 2026-11-05
  End date   (YYYY-MM-DD): 2026-11-09
  Reservation confirmed! Pay on delivery.
```

Reservations that overlap an existing booking for the same car are
rejected by the double-booking guard. Renewing is refused when another
customer is already waiting in that car's queue — first come, first
served.

## Staff menu

| Option | What it does |
|---|---|
| 1. View fleet | fleet table |
| 2. Search car by name | quick lookup |
| 3. Deliver car to customer | hand a RESERVED car over → status `RENTED` |
| 4. Process car return (with penalty) | invoice + late fees, car returns to the fleet |
| 5. Send car to maintenance | take an idle car out of service |
| 6. Finish maintenance | put the car back into `AVAILABLE` |
| 7. Show maintenance history | full log: date, description, cost |
| 8. Logout | back to the main menu |

The return flow asks for the **actual return date**; every day past the
agreed end date is charged at **1.5 × the daily rate** and added to the
customer's balance.

## Manager menu

| Option | What it does |
|---|---|
| 1. Add car | register a new car (name, brand, price/day) |
| 2. View fleet (insertion order) | walks the doubly linked list |
| 3. View fleet (sorted by name) | walks the AVL tree index |
| 4. Search fleet by keyword | case-insensitive match on names **and** brands |
| 5. Fleet statistics | composition, utilization, revenue, top earner |
| 6. Fleet analytics dashboard | live bar charts: composition, rate buckets, bookings per car, next pickups, overdue returns |
| 7. Income report | per-car gross / maintenance / net + totals |
| 8. Export CSV & JSON reports | write `reports/fleet_report.csv`, `reports/bookings_report.csv` + `reports/report.json` |
| 9. Block user | lock an account immediately |
| 10. Unblock user | release a blocked account |
| 11. Create staff account | add a Staff login |
| 12. Save data to disk | flush state without leaving |
| 13. Save & logout | save and return to the main menu |
| 14. Exit program | save everything and quit |

Options 2 and 3 show the **same data through two different structures**
— insertion order via the doubly linked list, alphabetical via the AVL
index. The app also saves automatically on exit.

### Search, statistics and reports

**Keyword search (4)** answers "what Peugeots do we have?" without
remembering exact names — it lowercases both the keyword and every
car's name/brand and reports substring hits in insertion order.
`206`, `lx` and `benz` are all valid keywords; an empty keyword is
rejected by design so a blank Enter never dumps the whole fleet.

**Fleet statistics (5)** prints the snapshot the manager actually
needs before a business decision:

```text
  FLEET STATISTICS
  ------------------------------------------------
  Fleet size                     8 car(s)
  Available                            7
  ...
  Fleet utilization                12.5%
  Gross income                 1440.00
  NET revenue                  1440.00
  Top earner                   Benz C200
```

Utilization is the share of the fleet currently **rented or reserved**
(cars under maintenance count as idle — nothing is earning while
parked).

**Export CSV & JSON reports (8)** writes the same numbers to
spreadsheets — and one JSON file:

- `reports/fleet_report.csv` — one row per car, name-sorted
- `reports/bookings_report.csv` — one row per live booking, ordered by
  start date
- `reports/report.json` — the whole state as one JSON object
  (`stats` + `fleet` + `bookings`), dates as ISO strings, all strings
  escaped per RFC 8259 — safe to feed straight into any script,
  spreadsheet or dashboard

Both CSV files are plain CSV with a header line; the JSON layout is
sketched at the end of [DATA_FORMATS.md](DATA_FORMATS.md). The
`reports/` folder is created on demand and git-ignored, since it is
output, not state.

### Fleet analytics dashboard

**Fleet analytics dashboard (6)** renders the live state as ASCII bar
charts: fleet composition by status, the fleet split over daily-rate
buckets, bookings per car, the next five pickups with "in N days"
countdowns, and a warning line when bookings are past their end date.
The numbers are computed in the pure `Analytics` module — the same
figures the statistics screen and the JSON report carry, so all three
views can never drift apart.

## Maintenance

Cars that are `AVAILABLE` can be sent to maintenance; rented or reserved
cars are refused. Every intervention is appended to the car's history
log (day, description, cost) and feeds into the income report as a cost.

## Where is my data?

Three CSV files in `data/` next to the binary — see
[DATA_FORMATS.md](DATA_FORMATS.md) for the exact layout. Deleting the
`data/` folder resets the app; the demo seed returns on next start.

## Troubleshooting

- **Input re-asks forever?** Every prompt validates and re-asks until
  the value is correct; `Ctrl-D` (EOF) exits cleanly instead of looping.
- **Locked out?** Three wrong passwords block the account. Log in as a
  manager and use *Unblock user* (demo: `admin` / `manager123`).
- **Weird fleet state from experiments?** Stop the app, delete `data/`,
  start again — the demo fleet is re-seeded.
