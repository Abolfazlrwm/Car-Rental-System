/**
 * ============================================================
 *  Container benchmarks — hand-written vs the standard library
 * ============================================================
 *  Deterministic (fixed seed), single-threaded, one run per
 *  scenario, built with -O2. `make bench` / `./bench_containers`
 *  — never run by CI; these numbers are for humans.
 *
 *  Scenarios mirror how the app actually uses each structure:
 *    - AVLTree          car index by name (int stand-ins for keys)
 *    - HashTable        username index (string keys, like the app)
 *    - PriorityQueue    booking schedule (insert + drain)
 *    - DoublyLinkedList insertion-order fleet list
 *
 *  The AVL tree has no erase by design (the app never deletes
 *  cars), so instead of an erase race it shows the height bound
 *  after the mass insert — the property that makes it AVL.
 * ============================================================
 */

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <functional>
#include <list>
#include <map>
#include <queue>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "../basic_structures/AVLTree.h"
#include "../basic_structures/DoublyLinkedList.h"
#include "../basic_structures/HashTable.h"
#include "../basic_structures/PriorityQueue.h"

namespace
{
template <class Fn> double measure(Fn &&fn)
{
    const auto t0 = std::chrono::steady_clock::now();
    fn();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

void printRow(const char *scenario, double oursMs, double stdMs)
{
    std::cout << "  " << std::left << std::setw(44) << scenario << std::right
              << std::fixed << std::setprecision(1) << std::setw(11)
              << oursMs << " ms" << std::setw(11) << stdMs << " ms"
              << std::setw(8) << std::setprecision(2) << oursMs / stdMs
              << "x\n";
}

std::vector<int> shuffledRange(int n)
{
    std::vector<int> keys(n);
    for (int i = 0; i < n; ++i)
        keys[i] = i;
    std::mt19937 rng(12345); // fixed seed -> reproducible numbers
    std::shuffle(keys.begin(), keys.end(), rng);
    return keys;
}

uint64_t g_sink = 0; // keeps -O2 from erasing the benchmarked work
} // namespace

int main()
{
    const int kMapN = 400000;
    const int kLookupN = 200000;
    const int kHeapN = 500000;
    const int kListN = 400000;

    const std::vector<int> shuffled = shuffledRange(kMapN);
    const std::vector<int> lookups = shuffledRange(kLookupN);

    std::cout << "==== Car Rental System - container benchmarks ====\n"
              << "  (one run per scenario, fixed seed 12345, -O2)\n\n"
              << "  " << std::left << std::setw(44) << "scenario"
              << std::right << std::setw(14) << "ours" << std::setw(14)
              << "std" << std::setw(9) << "ratio\n"
              << "  " << std::string(81, '-') << "\n";

    /* ---- AVLTree vs std::map (int keys) ---------------------------- */
    double oursMs = 0.0, stdMs = 0.0;

    oursMs = measure([&]
                     {
                         AVLTree<int, int> tree;
                         for (int k : shuffled)
                             tree.insert(k, k);
                         g_sink += static_cast<uint64_t>(tree.height());
                     });
    stdMs = measure([&]
                    {
                        std::map<int, int> m;
                        for (int k : shuffled)
                            m.emplace(k, k);
                        g_sink += m.size();
                    });
    printRow("AVLTree vs std::map: insert 400k shuffled", oursMs, stdMs);

    oursMs = measure([&]
                     {
                         AVLTree<int, int> tree;
                         for (int k : shuffled)
                             tree.insert(k, k);
                         uint64_t hits = 0;
                         for (int i = 0; i < kLookupN; ++i)
                             if (tree.contains((i & 1) ? lookups[i]
                                                       : lookups[i] % kMapN))
                                 ++hits;
                         g_sink += hits;
                     });
    stdMs = measure([&]
                    {
                        std::map<int, int> m;
                        for (int k : shuffled)
                            m.emplace(k, k);
                        uint64_t hits = 0;
                        for (int i = 0; i < kLookupN; ++i)
                            if (m.count((i & 1) ? lookups[i]
                                                : lookups[i] % kMapN))
                                ++hits;
                        g_sink += hits;
                    });
    printRow("AVLTree vs std::map: 200k mixed lookups", oursMs, stdMs);

    int ourHeight = 0;
    oursMs = measure([&]
                     {
                         AVLTree<int, int> tree;
                         for (int k : shuffled)
                             tree.insert(k, k);
                         ourHeight = tree.height();
                         tree.inOrderForEach(
                             [&](const int &, const int &v) { g_sink += v; });
                     });
    stdMs = measure([&]
                    {
                        std::map<int, int> m;
                        for (int k : shuffled)
                            m.emplace(k, k);
                        for (const auto &kv : m)
                            g_sink += kv.second;
                    });
    printRow("AVLTree vs std::map: full in-order walk", oursMs, stdMs);
    std::cout << "  " << std::left << std::setw(44)
              << "  height after 400k inserts (AVL bound ~25)"
              << std::right << std::setw(11) << ourHeight << std::setw(12)
              << "red-black" << "\n";

    /* ---- HashTable vs std::unordered_map (string keys, like users) -- */
    std::vector<std::string> userKeys;
    userKeys.reserve(kMapN);
    for (int i = 0; i < kMapN; ++i)
        userKeys.push_back("user_" + std::to_string(i));

    oursMs = measure([&]
                     {
                         HashTable<std::string, int> table;
                         for (int i = 0; i < kMapN; ++i)
                             table.insert(userKeys[i], i);
                         g_sink += table.size();
                     });
    stdMs = measure([&]
                    {
                        std::unordered_map<std::string, int> m;
                        m.reserve(kMapN);
                        for (int i = 0; i < kMapN; ++i)
                            m.emplace(userKeys[i], i);
                        g_sink += m.size();
                    });
    printRow("HashTable vs unordered_map: 400k strings", oursMs, stdMs);

    oursMs = measure([&]
                     {
                         HashTable<std::string, int> table;
                         for (int i = 0; i < kMapN; ++i)
                             table.insert(userKeys[i], i);
                         uint64_t hits = 0;
                         for (int i = 0; i < kLookupN; ++i)
                             if (table.find(userKeys[(i & 1) ? i
                                                             : kMapN - 1 - i]))
                                 ++hits;
                         g_sink += hits;
                     });
    stdMs = measure([&]
                    {
                        std::unordered_map<std::string, int> m;
                        m.reserve(kMapN);
                        for (int i = 0; i < kMapN; ++i)
                            m.emplace(userKeys[i], i);
                        uint64_t hits = 0;
                        for (int i = 0; i < kLookupN; ++i)
                            if (m.count(userKeys[(i & 1) ? i : kMapN - 1 - i]))
                                ++hits;
                        g_sink += hits;
                    });
    printRow("HashTable vs unordered_map: 200k lookups", oursMs, stdMs);

    oursMs = measure([&]
                     {
                         HashTable<std::string, int> table;
                         for (int i = 0; i < kMapN; ++i)
                             table.insert(userKeys[i], i);
                         for (int i = 0; i < kLookupN; ++i)
                             table.remove(userKeys[i * 2]);
                         g_sink += table.size();
                     });
    stdMs = measure([&]
                    {
                        std::unordered_map<std::string, int> m;
                        m.reserve(kMapN);
                        for (int i = 0; i < kMapN; ++i)
                            m.emplace(userKeys[i], i);
                        for (int i = 0; i < kLookupN; ++i)
                            m.erase(userKeys[i * 2]);
                        g_sink += m.size();
                    });
    printRow("HashTable vs unordered_map: 200k erases", oursMs, stdMs);

    /* ---- PriorityQueue vs std::priority_queue (min-heap) ------------ */
    const std::vector<int> heapKeys = shuffledRange(kHeapN);
    oursMs = measure([&]
                     {
                         PriorityQueue<int> pq(
                             [](const int &a, const int &b)
                             { return a < b; });
                         for (int k : heapKeys)
                             pq.insert(k);
                         uint64_t sum = 0;
                         while (!pq.empty())
                             sum += static_cast<uint64_t>(pq.extractMin());
                         g_sink += sum;
                     });
    stdMs = measure([&]
                    {
                        std::priority_queue<int, std::vector<int>,
                                            std::greater<int>>
                            pq;
                        for (int k : heapKeys)
                            pq.push(k);
                        uint64_t sum = 0;
                        while (!pq.empty())
                        {
                            sum += static_cast<uint64_t>(pq.top());
                            pq.pop();
                        }
                        g_sink += sum;
                    });
    printRow("PriorityQueue vs std: 500k push + drain", oursMs, stdMs);

    /* ---- DoublyLinkedList vs std::list ------------------------------ */
    oursMs = measure([&]
                     {
                         DoublyLinkedList<int> list;
                         for (int i = 0; i < kListN; ++i)
                             list.pushBack(i);
                         g_sink += list.size();
                     });
    stdMs = measure([&]
                    {
                        std::list<int> l;
                        for (int i = 0; i < kListN; ++i)
                            l.push_back(i);
                        g_sink += l.size();
                    });
    printRow("DoublyLinkedList vs std::list: 400k pushBack", oursMs, stdMs);

    oursMs = measure([&]
                     {
                         DoublyLinkedList<int> list;
                         for (int i = 0; i < kListN; ++i)
                             list.pushBack(i);
                         uint64_t sum = 0;
                         list.forEach([&](const int &v) { sum += v; });
                         g_sink += sum;
                     });
    stdMs = measure([&]
                    {
                        std::list<int> l;
                        for (int i = 0; i < kListN; ++i)
                            l.push_back(i);
                        uint64_t sum = 0;
                        for (int v : l)
                            sum += v;
                        g_sink += sum;
                    });
    printRow("DoublyLinkedList vs std::list: forward walk", oursMs, stdMs);

    oursMs = measure([&]
                     {
                         DoublyLinkedList<int> list;
                         for (int i = 0; i < kListN; ++i)
                             list.pushBack(i);
                         for (int i = 0; i < kListN / 2; ++i)
                             list.popFront();
                         g_sink += list.size();
                     });
    stdMs = measure([&]
                    {
                        std::list<int> l;
                        for (int i = 0; i < kListN; ++i)
                            l.push_back(i);
                        for (int i = 0; i < kListN / 2; ++i)
                            l.pop_front();
                        g_sink += l.size();
                    });
    printRow("DoublyLinkedList vs std::list: 200k popFront", oursMs, stdMs);

    std::cout << "\n  (ratio < 1.00 means the hand-written container was\n"
              << "   faster; anti-optimization sink: " << g_sink << ")\n";
    return 0;
}
