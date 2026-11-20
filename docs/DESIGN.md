# DESIGN — data-structure choices and complexity trade-offs

This document explains *why* each hand-written container exists, what job it does,
and which trade-offs were accepted. Implementations live in `basic_structures/`;
micro-benchmarks in `benchmarks/bench_containers.cpp`; the numbered results in
`docs/BENCHMARKS.md`.

## The four containers and their jobs

| Structure | Used for | Key operations | Complexity | Why this one |
|---|---|---|---|---|
| `AVLTree<K,V>` | car index by name | insert, find, in-order traversal (sorted fleet view) | O(log n) **guaranteed** (height-balanced) | Sorted views need order *and* worst-case guarantees; a plain BST degenerates to O(n) on sorted input |
| `HashTable<K,V>` | username → user index | insert, find, remove | O(1) average; auto-rehash at load factor 0.75 | Login/auth happens on every request — constant-time exact-key lookup beats the tree's log n |
| `PriorityQueue<T>` | per-car booking schedule | insert, peek, extractMin | O(log n) | Earliest-pickup-first scheduling falls out of the heap property, with a FIFO tie-break for fairness |
| `DoublyLinkedList<T>` | insertion-order fleet list | push front/back, remove, traversal | O(1) push, O(1) remove with the node | Preserves insertion order without re-sorting; O(1) removal given the node |

## Why hand-written instead of `std::map` / `std::unordered_map`

This is a data-structures course project: the point is to *build* the containers,
not to use them. The rule applied throughout the codebase: **`std::` containers
are fine in application code; everything in `basic_structures/` must be
implemented from scratch and unit-tested.**

The cost of that decision is real and acknowledged:

- No allocator tuning or cache-friendly layout like production-grade containers.
- The rebalancing and rehash logic is ours to prove correct — which is why the
  suite is large (**5,416 CI-run checks**, ASan/UBSan clean).
- Standard containers would win raw throughput benchmarks; the AVL worst-case
  guarantee is the interesting property here, not speed records.

## The adversarial case: sorted input

The classic failure mode of a naive BST is sorted insertion — exactly what a car
index by name invites (alphabetical bulk imports). The AVL rotation logic is
therefore tested against the worst case explicitly: inserting 500 keys in
ascending order must keep the height logarithmic. The same scenario runs in
`benchmarks/bench_containers.cpp` next to the happy paths.

## Hash table policy

Rehash triggers at load factor **0.75** — the usual balance between chain length
(lookup cost) and rehash frequency (insert-time spikes). The table is generic
(`HashTable<K,V>`) and serves the username index; the AVL serves the name-ordered
car index, so lookup-by-key and browse-in-order each use the structure that fits.

## Domain rules built on the containers

- **Double-booking prevention** — a reservation is only accepted against a car's
  priority queue if the requested range does not overlap existing bookings; the
  car lifecycle state machine is `AVAILABLE → RESERVED → RENTED → MAINTENANCE`.
- **Earliest-pickup-first scheduling** — the per-car heap serves the earliest
  pickup, FIFO on ties.
- **Waitlist with auto-booking** — when requested dates are taken, a wish is
  filed on the booked car; when a slot frees, the facade books the earliest wish
  automatically (`data/waitlist.csv` persists the queue).
- **Audit trail** — manager actions land in the audit log (`manager_system/AuditLog.*`),
  kept alongside the same container infrastructure.
- **Credential storage** — salted SHA-256 password hashing implemented from
  scratch (`manager_system/SHA256.*`, FIPS 180-4, dependency-free).

## Persistence model

Full state round-trips through `data/*.csv`: users (salted hashes, balances,
blocked flags), cars, bookings and the waitlist. The format details — including
the JSON exports used by the loyalty/reporting features — are documented in
`docs/DATA_FORMATS.md`.

## What I would do differently in production

For a real product I would reach for `std::map` / `std::unordered_map` /
`std::priority_queue` (or a real database) and spend the saved effort on
distribution, backups and monitoring. Hand-rolled containers here are the
*learning artifact*, deliberately isolated behind their own headers so the rest
of the code could switch to standard containers with minimal churn.
