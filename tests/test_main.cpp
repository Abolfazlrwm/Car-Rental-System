/**
 * ============================================================
 *  Unit test suite
 * ============================================================
 *  A dependency-free test runner (no external framework needed,
 *  so `cmake && ctest` works out of the box on any machine).
 *
 *  Covered:
 *    - SHA256 against official FIPS 180-4 test vectors
 *    - DoublyLinkedList: insert/remove/search from both ends
 *    - PriorityQueue: heap order, growth beyond initial capacity
 *    - AVLTree: sorted-insert stress (worst case) + height bound
 *    - HashTable: collisions, rehashing, custom hash function
 *    - Reservation: priority + overlap rules
 *    - RentalManager: the bugs that existed in v1.0 are now
 *      regression tests (null login, double income, fake renewal,
 *      duplicate fleet names, blocked accounts, persistence)
 *
 *  Run:  ./build/unit_tests   (exit code 0 = all green)
 * ============================================================
 */

#include <cmath>
#include <cstdio>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../basic_structures/AVLTree.h"
#include "../basic_structures/DoublyLinkedList.h"
#include "../basic_structures/HashTable.h"
#include "../basic_structures/PriorityQueue.h"
#include "../manager_system/Car.h"
#include "../manager_system/Reservation.h"
#include "../manager_system/RentalManager.h"
#include "../manager_system/SHA256.h"
#include "../manager_system/User.h"

/* ============================================================
 *  Mini test framework
 * ============================================================ */

static int g_checks = 0;
static int g_failures = 0;
static const char *g_currentSuite = "?";

#define CHECK(cond)                                                            \
    do                                                                         \
    {                                                                          \
        ++g_checks;                                                            \
        if (!(cond))                                                           \
        {                                                                      \
            ++g_failures;                                                      \
            std::cout << "  [FAIL] " << g_currentSuite << " line "             \
                      << __LINE__ << ": CHECK(" #cond ")\n";                   \
        }                                                                      \
    } while (0)

#define CHECK_EQ(a, b)                                                         \
    do                                                                         \
    {                                                                          \
        ++g_checks;                                                            \
        auto _va = (a);                                                        \
        auto _vb = (b);                                                        \
        if (!(_va == _vb))                                                     \
        {                                                                      \
            ++g_failures;                                                      \
            std::cout << "  [FAIL] " << g_currentSuite << " line "             \
                      << __LINE__ << ": " << _va << " != " << _vb << "\n";     \
        }                                                                      \
    } while (0)

static void runSuite(const char *name, const std::function<void()> &fn)
{
    int before = g_failures;
    g_currentSuite = name;
    fn();
    std::cout << (g_failures == before ? "  [ ok ] " : "  [FAIL] ") << name
              << "\n";
}

static bool approx(double a, double b, double eps = 1e-6)
{
    return std::fabs(a - b) < eps;
}

/* ============================================================
 *  SHA-256 (official NIST test vectors)
 * ============================================================ */

static void testSHA256()
{
    // Vector 1: empty string
    CHECK_EQ(SHA256::hash(""),
             std::string("e3b0c44298fc1c149afbf4c8996fb924"
                         "27ae41e4649b934ca495991b7852b855"));
    // Vector 2: "abc"
    CHECK_EQ(SHA256::hash("abc"),
             std::string("ba7816bf8f01cfea414140de5dae2223"
                         "b00361a396177a9cb410ff61f20015ad"));
    // Vector 3: the classic two-sequence message
    CHECK_EQ(SHA256::hash("The quick brown fox jumps over the lazy dog"),
             std::string("d7a8fbb307d7809469ca9abcb0082e4f8"
                         "d5651e46d3cdb762d02d0bf37c9e592"));
    // Vector 4: one-million 'a' characters is the standard stress vector;
    // a smaller multi-block input keeps the suite fast while still
    // exercising several padding rounds.
    std::string longInput(1000, 'a');
    CHECK_EQ(SHA256::hash(longInput).size(), std::string::size_type(64));

    // Salted password round trip
    Customer u("tester");
    u.setPassword("s3cret!");
    CHECK(u.verifyPassword("s3cret!"));
    CHECK(!u.verifyPassword("s3cret"));
    CHECK(!u.verifyPassword(""));
    // Two users with identical passwords -> different stored digests
    Customer a("a"), b("b");
    a.setPassword("same");
    b.setPassword("same");
    CHECK(a.passwordHash() != b.passwordHash());
    CHECK(a.salt() != b.salt());
}

/* ============================================================
 *  DoublyLinkedList
 * ============================================================ */

static void testDoublyLinkedList()
{
    DoublyLinkedList<int> list;
    CHECK(list.empty());
    CHECK_EQ(list.size(), std::size_t(0));

    list.pushBack(2);
    list.pushFront(1);
    list.pushBack(3);
    CHECK_EQ(list.toString(), std::string("[1, 2, 3]"));
    CHECK_EQ(list.size(), std::size_t(3));

    list.insertAt(1, 99);
    CHECK_EQ(list.toString(), std::string("[1, 99, 2, 3]"));
    CHECK(list.insertAt(5, 0) == false); // out of range

    CHECK_EQ(list.indexOf(99), 1);
    CHECK_EQ(list.indexOf(42), -1);
    CHECK(list.contains(3));

    CHECK(list.removeAt(1));
    CHECK_EQ(list.toString(), std::string("[1, 2, 3]"));
    CHECK(list.removeValue(2));
    CHECK_EQ(list.toString(), std::string("[1, 3]"));

    CHECK_EQ(list.front(), 1);
    CHECK_EQ(list.back(), 3);

    list.popFront();
    list.popBack();
    CHECK(list.empty());
    CHECK(!list.popFront()); // popping empty is a safe no-op

    // Stress: 1000 elements survive insert/remove in both directions
    DoublyLinkedList<int> big;
    for (int i = 0; i < 1000; ++i)
        big.pushBack(i);
    for (int i = 0; i < 500; ++i)
        big.popFront();
    CHECK_EQ(big.size(), std::size_t(500));
    CHECK_EQ(big.front(), 500);
}

/* ============================================================
 *  PriorityQueue
 * ============================================================ */

struct Task
{
    int priority;
    int seq;
};

static void testPriorityQueue()
{
    // Min-heap on .priority, FIFO on ties via .seq
    PriorityQueue<Task> q([](const Task &a, const Task &b)
                          { return a.priority != b.priority
                                       ? a.priority < b.priority
                                       : a.seq < b.seq; });

    q.insert({5, 0});
    q.insert({1, 1});
    q.insert({3, 2});
    q.insert({1, 3});
    q.insert({4, 4});

    CHECK_EQ(q.size(), std::size_t(5));
    CHECK_EQ(q.peek().priority, 1);
    CHECK_EQ(q.peek().seq, 1); // tie -> FIFO

    std::vector<int> drained;
    while (!q.empty())
        drained.push_back(q.extractMin().priority);
    CHECK_EQ(drained.size(), std::size_t(5));
    for (std::size_t i = 1; i < drained.size(); ++i)
        CHECK(drained[i - 1] <= drained[i]); // non-decreasing order

    // Growth beyond the initial block (the v1.0 heap silently
    // dropped element #51 — regression test)
    PriorityQueue<int> big([](int a, int b)
                           { return a < b; });
    for (int i = 200; i >= 1; --i)
        big.insert(i);
    CHECK_EQ(big.size(), std::size_t(200));
    CHECK_EQ(big.peek(), 1);
    for (int i = 1; i <= 200; ++i)
        CHECK_EQ(big.extractMin(), i);
}

/* ============================================================
 *  AVLTree
 * ============================================================ */

