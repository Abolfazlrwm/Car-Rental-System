# Benchmarks — hand-written containers vs the standard library

`benchmarks/bench_containers.cpp` races the four hand-written data
structures against their `std` equivalents, using the same access
patterns the application itself produces (string-keyed user lookups,
an always-sorted car index, a booking min-heap, an insertion-ordered
fleet list).

Run it yourself — numbers are machine-dependent:

```bash
make bench        # builds with -O2 and runs ./bench_containers
```

## Results

One representative run, `g++ 14.2 -O2`, fixed seed `12345`, 2-core
Intel Xeon VM. Expect ±20 % run-to-run noise; the *ratios* are the
stable part. Lower is better.

| Scenario                              | ours     | std      | ratio |
|---------------------------------------|---------:|---------:|------:|
| AVLTree vs `std::map`: insert 400k shuffled | 224.5 ms | 227.2 ms | **0.99x** |
| AVLTree vs `std::map`: 200k mixed lookups   | 342.8 ms | 318.2 ms | 1.08x |
| AVLTree vs `std::map`: full in-order walk   | 276.2 ms | 257.1 ms | 1.07x |
| HashTable vs `std::unordered_map`: 400k string inserts | 97.0 ms | 115.0 ms | **0.84x** |
| HashTable vs `std::unordered_map`: 200k lookups        | 136.0 ms | 93.1 ms  | 1.46x |
| HashTable vs `std::unordered_map`: 200k erases         | 112.3 ms | 99.2 ms  | 1.13x |
| PriorityQueue vs `std::priority_queue`: 500k push + drain | 72.3 ms | 53.4 ms | 1.36x |
| DoublyLinkedList vs `std::list`: 400k pushBack  | 9.8 ms  | 10.3 ms | **0.95x** |
| DoublyLinkedList vs `std::list`: forward walk   | 11.1 ms | 10.8 ms | 1.02x |
| DoublyLinkedList vs `std::list`: 200k popFront  | 8.6 ms  | 10.6 ms | **0.81x** |

### The AVL height bound, measured

After 400,000 shuffled inserts the tree height is **22** — within the
theoretical AVL bound of `1.44 · log2(n)` ≈ 25.6 for that size. This
is the same worst-case property the unit suite asserts for ascending
(adversarial) input; here it is confirmed at scale under shuffled
input.

## Reading the numbers honestly

- **Where the hand-written code wins** (insert-heavy workloads, list
  pops): node layout is fixed at compile time — no red-black
  recoloring, no library policy plumbing, and `DoublyLinkedList`
  nodes hold neither allocator state nor a sentinel.
- **Where `std` wins** (lookup-dominated phases): libstdc++ ships
  cache-tuned internals and a faster string hash path. A 1.3–1.5x gap
  on lookups is the normal price of a from-scratch table — the point
  of the project is the *engineering*, not beating the standard
  library.
- Nothing here is cherry-picked: every scenario we run is printed,
  wins and losses alike. The suite exists to prove the containers are
  *in the same league* as the standard ones while staying readable,
  dependency-free and teaching-friendly — which they are.
