# 🚗 Car Rental Management System — C++17

![CI](https://img.shields.io/badge/CI-passing-brightgreen)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)
![Tests](https://img.shields.io/badge/tests-2%2C117%20checks-3AAFA9)
![Memory](https://img.shields.io/badge/AddressSanitizer-leak--free-4C6EF5)
![License](https://img.shields.io/badge/license-MIT-64748B)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey)

A complete car-rental workflow (customers, staff, manager) built on
**four hand-written generic data structures** — an AVL tree, a hash table
with rehashing, a binary heap and a doubly linked list. Every container is
implemented from scratch, unit-tested, and documented; the application on
top is a layered, leak-free C++17 console system with real password
hashing, double-booking prevention, late-return penalties and full CSV
persistence.

> Originally written as a data-structures course project, then rebuilt
> into a portfolio-grade repository — see [`docs/IMPROVEMENTS.md`](docs/IMPROVEMENTS.md)
> for the full changelog (fixed segfaults, memory leaks, an
> income double-count and more).

---

## 🏗 Architecture

![](docs/architecture.png)

```
mainApp.cpp (Console UI)      role menus · input guards · colored tables
        │
RentalManager (Facade)        business rules · invoicing · persistence
        │
basic_structures/ (Templates) AVLTree<K,V> · HashTable<K,V> ·
                              PriorityQueue<T> · DoublyLinkedList<T>
```

### The data structures — and where they earn their keep

| Structure | Used for | Key operations | Complexity |
|-----------|----------|----------------|------------|
| `AVLTree<K,V>` | car index by name | insert, find, in-order (sorted fleet view) | O(log n) guaranteed |
| `HashTable<K,V>` | username index | insert, find, remove, **auto-rehash at load factor 0.75** | O(1) average |
| `PriorityQueue<T>` | per-car booking schedule | insert, peek, extractMin — earliest pickup served first | O(log n) |
| `DoublyLinkedList<T>` | insertion-order fleet list | push front/back, remove, traversal | O(1) push |

All four are **header-only templates** with injectable comparators and an
explicit ownership model (`ownsValues`), covered by 2,100+ assertions —
including the worst-case test: inserting 500 ascending keys into the AVL
tree and asserting the height stays within the theoretical bound.

---

## ✨ Features

**Accounts & security**
- Customer self-registration; Manager creates Staff/Manager accounts
- **Salted SHA-256** password storage (implemented from scratch, verified
  against official NIST test vectors)
- Auto-block after 3 failed logins, Manager can unblock

**Fleet & reservations**
- Car lifecycle: `AVAILABLE → RESERVED → RENTED → MAINTENANCE`
- Double-booking prevention (date-range overlap rejection)
- Earliest-pickup-first scheduling (priority queue + FIFO tie-break)
- Rental renewal with queue discipline (blocked when others wait)

**Money & reporting**
- Automatic invoicing: duration × daily price
- **Late-return penalty**: 1.5 × daily rate per late day
- Customer balances with one-tap bill payment
- Income report per car + net revenue (income − maintenance costs)

**Maintenance**
- Send to / return from maintenance, blocked while rented
- Queryable history log with date, description and cost

**Persistence**
- Full state round-trip: users (salted hashes, balances, blocked flags),
  cars (status, income) and **live reservations** in `data/*.csv`
- Auto-seeded demo fleet on first run — clone, build, and it just works

---

## 🎬 Demo

Reserve the Benz → staff delivers it → customer returns it 2 days late →
the manager sees the penalty on the books:

```text
$ ./scripts/demo.sh

===== CUSTOMER MENU | ali | balance due: 0.00 =====
  Choice:   Car name:   Start day (number):   End day   (number):
  Reservation confirmed! Pay on delivery.

  YOUR RESERVATIONS
  #1 | Benz C200        | days 5 -> 9 (5 day(s))

===== STAFF MENU | sara =====
  Choice:   Car name:   Car handed over. Drive safe!
  Choice:   Car name:   Actual return day:   Return processed and invoiced.

===== MANAGER MENU | admin =====
  ----------------- INCOME REPORT -----------------
  CAR                   INCOME   MAINTENANCE         NET
  ------------------------------------------------------
  Pride 131               0.00          0.00        0.00
  ...
  Benz C200            1440.00          0.00     1440.00   <- 5x180 + 2 late days x 1.5x180
  ------------------------------------------------------
    Gross income              1440.00
    NET revenue               1440.00
```

Run it yourself:

```bash
./scripts/demo.sh          # scripted end-to-end walkthrough
```

**Demo accounts** (auto-seeded on first run):

| Role | Username | Password |
|------|----------|----------|
| Manager | `admin` | `manager123` |
| Staff | `sara` | `staff123` |
| Customer | `ali` | `customer123` |

---

## 🚀 Build & Run

**Requirements:** a C++17 compiler (GCC ≥ 8, Clang ≥ 8, MSVC 2019+)

### Option 1 — Make (quickest)

```bash
make            # builds car_rental + unit_tests
./car_rental    # start the app
make test       # run the unit tests
```

### Option 2 — CMake

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/car_rental
```

### Option 3 — one-liner

```bash
g++ -std=c++17 -O2 mainApp.cpp manager_system/*.cpp -o car_rental && ./car_rental
```

On first start the app seeds the demo fleet and accounts; all state is
saved to `data/` on exit and reloaded on the next run.

---

## 📁 Project structure

```
Car-Rental-System/
├── mainApp.cpp              # console UI: menus, input guards, colors
├── manager_system/          # business layer
│   ├── RentalManager.{h,cpp}# facade: rules, invoicing, persistence
│   ├── User.{h,cpp}         # User → Customer / Staff / Manager
│   ├── Car.{h,cpp}          # status, income ledger, maintenance log
│   ├── Reservation.{h,cpp}  # booking + priority rule
│   └── SHA256.{h,cpp}       # FIPS 180-4, dependency-free
├── basic_structures/        # hand-written generic containers (header-only)
│   ├── AVLTree.h            # self-balancing BST (LL/RR/LR/RL rotations)
│   ├── HashTable.h          # separate chaining + rehashing
│   ├── PriorityQueue.h      # binary min-heap, auto-growing
│   └── DoublyLinkedList.h   # O(1) pushes at both ends
├── tests/test_main.cpp      # 2,117 checks, dependency-free framework
├── scripts/demo.sh          # scripted walkthrough
├── docs/
│   ├── IMPROVEMENTS.md      # full v1.0 → v2.0 changelog
│   └── architecture.png     # diagram used above
├── CMakeLists.txt / Makefile
└── .github/workflows/ci.yml # GCC + Clang matrix on every push
```

---

## 🧪 Testing

The suite needs no external framework and runs in milliseconds:

```text
==== Car Rental System — unit tests ====
  [ ok ] SHA256 + password hashing
  [ ok ] DoublyLinkedList
  [ ok ] PriorityQueue
  [ ok ] AVLTree (self-balancing)
  [ ok ] HashTable (chaining + rehash)
  [ ok ] Reservation rules
  [ ok ] RentalManager: accounts & lockout
  [ ok ] RentalManager: reservations & invoices
  [ ok ] RentalManager: fleet & maintenance
  [ ok ] RentalManager: CSV persistence
  [ ok ] Role hierarchy

2117 checks, 0 failure(s)
ALL TESTS PASSED
```

Notable regression tests — each one reproduces a bug that existed in
v1.0: segfault on unknown login, income double-count, fake renewal,
self-registered managers, silent drops on a full heap.

Memory safety is verified with `g++ -fsanitize=address,undefined`.

---

## 🗺 Roadmap

- [ ] Real calendar dates (`std::chrono`) instead of day numbers
- [ ] Text-based UI using ncurses (windows, colors, arrow-key navigation)
- [ ] JSON export in addition to CSV
- [ ] Fine-grained permissions inside the Staff/Manager roles
- [ ] Docker devcontainer for one-click contributor setup

## 🤝 Contributing

Issues and PRs are welcome — the test suite keeps refactors safe.
Run `make test` before pushing.

## 📄 License

[MIT](LICENSE)
