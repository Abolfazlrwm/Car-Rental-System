# Improvement Log — v1.0 → v2.0

This document lists every bug found in the original codebase and every
improvement made while turning it into a portfolio-grade project.
Each fix is covered by a regression test in `tests/test_main.cpp`.

---

## 1. Critical bugs fixed

| # | Bug in v1.0 | Severity | Fix in v2.0 |
|---|-------------|----------|-------------|
| 1 | `login()` called `u->getUsername()` **without checking for nullptr** — logging in with an unknown username crashed the process (verified: `SIGSEGV`, exit code 139) | 🔴 Critical | Null-safe lookup; unknown user → clean error message. Regression-tested. |
| 2 | `cin >> choice` with no failure handling — any non-numeric input or EOF put the main loop into an **infinite loop** | 🔴 Critical | getline-based input layer: numeric prompts re-ask until valid, EOF exits cleanly. |
| 3 | **Income double-counted**: `addIncome(price)` fired at reservation time AND the full charge was added again at return — every revenue report was inflated | 🔴 Critical | Income is recorded exactly once, when the return is invoiced. Regression-tested. |
| 4 | `renewRental()` printed "Rental extended" but **never modified any date** | 🔴 Critical | Extends the active reservation's end date; refuses when other bookings are queued. |
| 5 | Anyone could self-register as `Manager` (any unrecognized role string fell into the `Manager` branch) | 🔴 Critical | Self-registration creates Customers only; Staff/Manager accounts are created by a logged-in Manager. |
| 6 | "Password hashing" was a Caesar cipher (`+3` per character) — trivially reversible | 🔴 Critical | Real SHA-256 (from scratch, FIPS 180-4) + per-user random salt; verified against official NIST test vectors. |
| 7 | `PriorityQueue` had a fixed capacity of 50 and **silently dropped** reservations when full | 🟠 Major | Dynamically growing heap (capacity doubles), fully tested up to 200 elements. |
| 8 | Memory leaks everywhere — cars, users, reservations and maintenance records were `new`ed and never `delete`d | 🟠 Major | Explicit single-owner model documented in `RentalManager.h`; leak-free under AddressSanitizer. |
| 9 | `HashTable::get()` declared but never defined; `payPenalty()` declared, never implemented | 🟠 Major | Dead declarations removed; the bill feature is fully implemented (balance, receipt, persistence). |
| 10 | `registerUser` created a `Customer` for role "Customer" but a `Manager` for *anything else*, including typos like "managr" | 🟠 Major | Role strings validated explicitly; unknown roles rejected. |
| 11 | Manager menu option "View cars" actually called `generateIncomeReport()` (mismatched menu/logic) | 🟡 Minor | Dedicated fleet views (insertion order + AVL-sorted) and a separate income report. |
| 12 | `cin >> string` input accepted single words only — car names/brands with spaces broke every screen | 🟡 Minor | Whole-line input everywhere; "Benz C200 GT" works as a car name. |
| 13 | CSV import skipped the overlap/status consistency and could insert duplicate cars | 🟡 Minor | Import goes through the same validation path as runtime operations. |

## 2. Architecture & code quality

- **Generic containers.** All four data structures were re-written as
  header-only templates: `AVLTree<K,V>`, `HashTable<K,V>`,
  `PriorityQueue<T>`, `DoublyLinkedList<T>`. The dependency direction is
  now clean: `basic_structures` no longer knows about cars or users.
- **Layered design.** Console UI (`mainApp.cpp`) → business facade
  (`RentalManager`) → containers. The business layer never prints to the
  console; the UI never touches containers directly.
- **Single-owner memory model.** Each heap object has exactly one owner
  (HashTable owns users, AVLTree owns cars, each Car owns its
  reservations). Verified with `-fsanitize=address`.
- **Modern C++17.** `enum class`, `override`, `if constexpr`,
  `std::filesystem`, `std::function`, range-based iteration via
  `forEach` callbacks, bounds-checked accessors that throw
  `std::out_of_range`.
- **Load-factor-aware hash table.** Buckets grow and rehash when the
  load factor passes 0.75 (v1.0 had a fixed 101 buckets).
- **Brute-force guard.** Three failed logins block the account; only a
  Manager can unblock.

## 3. New features

| Feature | Where |
|---------|-------|
| Customer balances & bill payment (rentals + penalties accumulate, one-tap pay) | `Customer`, customer menu |
| Late-return penalty (1.5×/day) with a real invoice: base + penalty | `returnCar()` |
| Maintenance lifecycle (send → finish → back in service) + queryable history log with costs | staff menu |
| Net revenue report (income − maintenance costs) | `printIncomeReport()` |
| Full persistence: users (salted hashes, blocked state, balances), cars (status, income) and **live reservations** round-trip through CSV | `saveData()/loadData()` |
| One-command demo: fresh clone auto-seeds fleet + 3 demo accounts | `seedDemoData()` |
| Car search by name (O(log n) index) and by brand | `searchCar()`, `findCarsByBrand()` |
| Sorted fleet view driven by the AVL in-order traversal | `forEachCarSorted()` |
| Colored terminal output that switches itself off when piped | `mainApp.cpp` |

## 4. Engineering around the code

| Addition | Purpose |
|----------|---------|
| `tests/test_main.cpp` — 2,100+ checks, zero dependencies | Regression-proofing every structure and business rule |
| `CMakeLists.txt` + `Makefile` | Two supported build paths (`cmake -B build`, `make`) |
| `.github/workflows/ci.yml` | GCC + Clang matrix, CTest + raw-compiler build on every push |
| `docs/architecture.png` | Layer diagram used in the README |
| `scripts/demo.sh` | Deterministic scripted walkthrough |
| `README.md`, `LICENSE` (MIT), `.gitignore` | Repository hygiene |

## 5. Metrics

| Metric | v1.0 | v2.0 |
|--------|------|------|
| Compiler warnings (`-Wall -Wextra`) | 1+ | 0 |
| Segfault on unknown login | yes | fixed |
| Income accuracy | double-counted | exact |
| Unit tests | none | 2,117 checks, all green |
| Memory leaks (ASan) | many | none detected |
| Persistence | cars only | users + cars + reservations |
| Build system | manual `g++` command | CMake + Make + CI |
