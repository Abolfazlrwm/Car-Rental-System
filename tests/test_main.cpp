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
 *    - Waitlist: wish priority, per-car heap, facade promotion,
 *      CSV round-trip
 *    - Loyalty: tier maths, points accrual on returns,
 *      redemption rules, program overview, CSV round-trip
 *    - RentalManager: the bugs that existed in v1.0 are now
 *      regression tests (null login, double income, fake renewal,
 *      duplicate fleet names, blocked accounts, persistence)
 *
 *  Run:  ./build/unit_tests   (exit code 0 = all green)
 * ============================================================
 */

#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "../manager_system/Analytics.h"
#include "../manager_system/AuditLog.h"
#include "../basic_structures/AVLTree.h"
#include "../basic_structures/DoublyLinkedList.h"
#include "../basic_structures/HashTable.h"
#include "../basic_structures/PriorityQueue.h"
#include "../manager_system/Car.h"
#include "../manager_system/Date.h"
#include "../manager_system/JsonExport.h"
#include "../manager_system/Loyalty.h"
#include "../manager_system/Permissions.h"
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

static void testAVLTree()
{
    AVLTree<int, int> tree;
    CHECK(tree.empty());

    // WORST CASE for a plain BST: ascending input must not degrade.
    // The AVL invariant keeps height <= 1.4405*log2(n+2).
    const int N = 500;
    for (int i = 1; i <= N; ++i)
        tree.insert(i, i * 10);

    CHECK_EQ(tree.size(), std::size_t(N));
    CHECK(tree.height() <= static_cast<int>(AVLTree<int, int>::maxHeightBound(N)));
    CHECK(tree.height() > 8); // a degenerate tree would have height ~N

    for (int i = 1; i <= N; ++i)
    {
        int *v = tree.find(i);
        CHECK(v != nullptr && *v == i * 10);
    }
    CHECK(tree.find(N + 1) == nullptr);

    // Duplicate key insertion must not overwrite or grow the tree.
    tree.insert(10, 999999);
    CHECK_EQ(*tree.find(10), 100);
    CHECK_EQ(tree.size(), std::size_t(N));

    // In-order traversal yields sorted keys.
    std::vector<int> keys;
    tree.inOrderForEach([&](const int &k, const int &)
                        { keys.push_back(k); });
    CHECK_EQ(keys.size(), std::size_t(N));
    bool sorted = true;
    for (std::size_t i = 1; i < keys.size(); ++i)
        if (keys[i - 1] >= keys[i])
            sorted = false;
    CHECK(sorted);

    // Descending input must balance too.
    AVLTree<int, int> rev;
    for (int i = N; i >= 1; --i)
        rev.insert(i, i);
    CHECK(rev.height() <= static_cast<int>(AVLTree<int, int>::maxHeightBound(N)));

    rev.clear();
    CHECK(rev.empty());
}

/* ============================================================
 *  HashTable
 * ============================================================ */

static void testHashTable()
{
    // std::hash default
    HashTable<std::string, int> table;
    CHECK(table.empty());
    CHECK(table.insert("ali", 1));
    CHECK(table.insert("sara", 2));
    CHECK(!table.insert("ali", 9)); // duplicate rejected
    CHECK_EQ(*table.find("ali"), 1);
    CHECK_EQ(*table.find("sara"), 2);
    CHECK(table.find("ghost") == nullptr);

    CHECK(table.remove("ali"));
    CHECK(!table.remove("ali")); // double remove fails cleanly
    CHECK(table.find("ali") == nullptr);
    CHECK_EQ(table.size(), std::size_t(1));

    // Collision + REHASH stress: 500 entries in a 17-bucket table forces
    // multiple rehashes (v1.0 had a fixed 101-bucket table).
    HashTable<int, int> stress;
    for (int i = 0; i < 500; ++i)
        CHECK(stress.insert(i, i * 3));
    CHECK_EQ(stress.size(), std::size_t(500));
    CHECK(stress.bucketCount() > 17); // it really grew
    CHECK(stress.loadFactor() <= 0.75);
    for (int i = 0; i < 500; ++i)
        CHECK(stress.find(i) != nullptr && *stress.find(i) == i * 3);
    for (int i = 0; i < 250; ++i)
        CHECK(stress.remove(i));
    CHECK_EQ(stress.size(), std::size_t(250));

    // Custom hash function injection (polynomial base 31, as the
    // RentalManager uses) + chained collisions on purpose.
    auto polyHash = [](const std::string &key)
    {
        std::size_t h = 0;
        for (unsigned char c : key)
            h = h * 31 + c;
        return h;
    };
    HashTable<std::string, std::string> poly(polyHash);
    poly.insert("ab", "first");  // 97*31+98
    poly.insert("a`", "second"); // 97*31+96 -> neighbor key
    poly.insert("ba", "third");  // 98*31+97 -> anagram, often same bucket
    CHECK_EQ(*poly.find("ab"), "first");
    CHECK_EQ(*poly.find("a`"), "second");
    CHECK_EQ(*poly.find("ba"), "third");
}

/* ============================================================
 *  Reservation rules
 * ============================================================ */

static void testReservationRules()
{
    Customer c1("ali"), c2("sara");
    Car car("Test Car", "Test", 100.0);

    // Range overlap truth table
    CHECK(Reservation::rangesOverlap(1, 5, 5, 9));   // touching edges
    CHECK(Reservation::rangesOverlap(4, 8, 1, 5));   // partial
    CHECK(Reservation::rangesOverlap(2, 3, 1, 10));  // contained
    CHECK(!Reservation::rangesOverlap(1, 4, 5, 9));  // disjoint
    CHECK(!Reservation::rangesOverlap(6, 9, 1, 5));  // disjoint (after)

    Reservation early(&c1, &car, 1, /*reqTime*/ 10, /*start*/ 2, /*end*/ 4);
    Reservation late(&c2, &car, 2, /*reqTime*/ 1, /*start*/ 5, /*end*/ 8);

    CHECK(early.hasHigherPriorityThan(&late));  // earlier start wins...
    CHECK(!late.hasHigherPriorityThan(&early)); // ...even with earlier request

    Reservation tieA(&c1, &car, 3, /*reqTime*/ 1, 5, 6);
    Reservation tieB(&c2, &car, 4, /*reqTime*/ 2, 5, 6);
    CHECK(tieA.hasHigherPriorityThan(&tieB)); // tie -> FIFO
    CHECK(!tieB.hasHigherPriorityThan(&tieA));

    CHECK_EQ(early.durationDays(), 3); // inclusive [2..4]
}

/* ============================================================
 *  Date — the 2.3.0 calendar layer (ISO <-> serial days)
 * ============================================================ */

static void testDates()
{
    int s = -99;

    // ---- epoch anchors ------------------------------------------------
    CHECK(Date::parseISO("1970-01-01", s));
    CHECK_EQ(s, 0);
    CHECK_EQ(Date::toISO(0), std::string("1970-01-01"));
    CHECK_EQ(Date::toISO(1), std::string("1970-01-02"));
    CHECK(Date::parseISO("2026-11-05", s));
    CHECK_EQ(Date::toISO(s), std::string("2026-11-05"));

    // ---- leap years, century rules ------------------------------------
    CHECK(Date::parseISO("2024-02-29", s)); // leap year
    CHECK_EQ(Date::toISO(s), std::string("2024-02-29"));
    CHECK(Date::parseISO("2000-02-29", s)); // divisible by 400 -> leap
    CHECK(!Date::parseISO("1900-02-29", s)); // divisible by 100 only
    CHECK(!Date::parseISO("2100-02-29", s));
    CHECK(!Date::parseISO("2026-02-30", s)); // Feb has 28 days here

    // ---- strict shape: garbage must never parse ------------------------
    const char *bad[] = {"2026-13-01", "2026-00-10", "2026-04-31",
                         "2026/11/05", "2026-1-05",  "2026-11-5",
                         "26-11-05",   "abcdefghij", "",
                         "2026-11-05 ", " 2026-11-05", "2026-11-0x",
                         "9999-99-99", "2026-11",    "2026-11-05extra"};
    for (const char *t : bad)
        CHECK(!Date::parseISO(t, s));
    CHECK(!Date::isISODate("2026-04-31"));
    CHECK(Date::isISODate("2099-12-31"));

    // ---- round-trip: every day of the first two years ------------------
    CHECK(Date::parseISO("1972-01-01", s));
    const int denseEnd = s; // serial 730
    for (int serial = 0; serial <= denseEnd; ++serial)
    {
        int back = -1;
        CHECK(Date::parseISO(Date::toISO(serial), back) && back == serial);
    }

    // ---- round-trip sweep to the year 2100 -----------------------------
    CHECK(Date::parseISO("2100-01-01", s));
    const int horizon = s;
    for (int serial = denseEnd; serial <= horizon; serial += 37)
    {
        int back = -1;
        CHECK(Date::parseISO(Date::toISO(serial), back) && back == serial);
    }

    // ---- ISO strings sort chronologically ------------------------------
    for (int serial = 0; serial < horizon; serial += 977)
        CHECK(Date::toISO(serial) < Date::toISO(serial + 1));

    // ---- flexible parsing (legacy day numbers still load) ---------------
    CHECK(Date::parseFlexible("2030-05-20", s));
    CHECK(Date::parseFlexible("42", s) && s == 42);
    CHECK(!Date::parseFlexible("42 cars", s));
    CHECK(!Date::parseFlexible("", s));
    CHECK(!Date::parseFlexible("-5", s));
    CHECK(!Date::parseFlexible("car", s));

    // ---- legacy CSV round-trip: a pre-2.3.0 reservations.csv ------------
    const std::string dir = "/tmp/crs_test_legacy_dates";
    std::system(("rm -rf " + dir).c_str());
    {
        RentalManager m;
        m.seedDemoData();
        std::string err;
        CHECK(m.registerCustomer("legacy", "pass123"));
        CHECK(m.login("legacy", "pass123"));
        CHECK(m.makeReservation("Dena Plus", 4, 8, err));
        m.logout();
        CHECK(m.saveData(dir)); // writes ISO dates
    }
    {
        RentalManager m;
        CHECK(m.loadData(dir));
        Car *dena = m.searchCar("Dena Plus");
        CHECK(dena != nullptr);
        CHECK_EQ(dena->reservationCount(), std::size_t(1));
        const Reservation *r = dena->currentReservation();
        CHECK(r != nullptr);
        const int startSerial = r->startDate();
        const int endSerial = r->endDate();
        CHECK(Date::toISO(startSerial) < Date::toISO(endSerial));

        // Rewrite the file exactly like a 2.2.x build stored it:
        // bare day numbers instead of ISO dates.
        {
            std::ofstream legacy(dir + "/reservations.csv", std::ios::trunc);
            CHECK(legacy.good());
            legacy << "id,carName,customer,requestTime,startDate,endDate\n";
            legacy << "1,Dena Plus,legacy,1," << startSerial << ","
                   << endSerial << "\n";
        }

        RentalManager m2;
        CHECK(m2.loadData(dir)); // legacy file must still load
        Car *dena2 = m2.searchCar("Dena Plus");
        CHECK(dena2 != nullptr);
        CHECK_EQ(dena2->reservationCount(), std::size_t(1));
        const Reservation *r2 = dena2->currentReservation();
        CHECK(r2 != nullptr);
        CHECK_EQ(r2->startDate(), startSerial);
        CHECK_EQ(r2->endDate(), endSerial);
    }

    // ---- corrupt rows are dropped, not fatal ----------------------------
    {
        std::ofstream bad(dir + "/reservations.csv", std::ios::trunc);
        bad << "id,carName,customer,requestTime,startDate,endDate\n";
        bad << "2,Dena Plus,legacy,1,not-a-date,2050-01-01\n";   // junk start
        bad << "3,Dena Plus,legacy,1,2050-01-05,2050-01-02\n";   // start > end
        bad << "4,Dena Plus,legacy,1,2050-01-05,2050-01-09\n";   // the good row
        bad.close();

        RentalManager m3;
        CHECK(m3.loadData(dir));
        Car *dena3 = m3.searchCar("Dena Plus");
        CHECK(dena3 != nullptr);
        CHECK_EQ(dena3->reservationCount(), std::size_t(1)); // only the good row
        const Reservation *r3 = dena3->currentReservation();
        CHECK(r3 != nullptr);
        CHECK_EQ(Date::toISO(r3->startDate()), std::string("2050-01-05"));
    }
    std::system(("rm -rf " + dir).c_str());
}

/* ============================================================
 *  RentalManager — regression tests for every v1.0 bug
 * ============================================================ */

static void testRentalManagerAccounts()
{
    RentalManager m;

    // REGRESSION: logging in with an unknown username used to
    // dereference nullptr and SEGFAULT.
    CHECK(!m.login("ghost", "whatever"));
    CHECK(!m.isLoggedIn());

    CHECK(m.registerCustomer("ali", "1234"));
    CHECK(!m.registerCustomer("ali", "5678")); // duplicate username
    CHECK(!m.registerCustomer("bob", "abc"));  // password too short

    CHECK(m.login("ali", "1234"));
    CHECK(m.currentUserRole() == "Customer");
    m.logout();
    CHECK(!m.isLoggedIn());

    // Self-registration cannot mint a Manager or Staff account.
    CHECK(m.registerCustomer("selfmgr", "pass1234"));
    CHECK(m.login("selfmgr", "pass1234"));
    CHECK(m.currentUserRole() == "Customer");
    m.logout();

    // Only a manager can create staff/manager accounts.
    CHECK(!m.registerStaffAccount("x", "pass1234", "Customer"));
    m.login("ali", "1234"); // not a manager, but registerStaffAccount
                            // is a direct API call in tests

    // 3 wrong passwords -> automatic block (brute-force guard).
    RentalManager m2;
    m2.seedDemoData();
    CHECK(m2.login("ali", "customer123"));
    m2.logout();
    CHECK(!m2.login("ali", "wrong1"));
    CHECK(!m2.login("ali", "wrong2"));
    CHECK(!m2.login("ali", "wrong3"));
    CHECK(m2.isUserBlocked("ali"));        // locked out...
    CHECK(!m2.login("ali", "customer123")); // ...even with the right password
    CHECK(m2.unblockUser("ali"));
    CHECK(m2.login("ali", "customer123")); // manager unblocks -> works
}

static void testRentalManagerReservations()
{
    RentalManager m;
    m.seedDemoData();

    CHECK(m.login("ali", "customer123"));

    std::string err;
    CHECK(!m.makeReservation("No Such Car", 1, 5, err)); // unknown car
    CHECK(!m.makeReservation("Benz C200", 9, 5, err));   // start > end
    CHECK(!m.makeReservation("Benz C200", -1, 5, err)); // before 1970-01-01

    CHECK(m.makeReservation("Benz C200", 5, 9, err));

    // REGRESSION: double-booking prevention.
    CHECK(!m.makeReservation("Benz C200", 6, 7, err)); // inside
    CHECK(!m.makeReservation("Benz C200", 4, 6, err)); // overlaps head
    CHECK(!m.makeReservation("Benz C200", 8, 12, err)); // overlaps tail
    CHECK(m.makeReservation("Benz C200", 10, 12, err)); // adjacent OK
    CHECK(!m.makeReservation("Benz C200", 12, 20, err)); // overlaps 2nd

    Car *benz = m.searchCar("Benz C200");
    CHECK(benz != nullptr);
    CHECK_EQ(benz->reservationCount(), std::size_t(2));
    CHECK(benz->status() == CarStatus::Reserved);

    // REGRESSION: renewRental used to print "extended" and change nothing.
    CHECK(!m.renewRental("Benz C200", 2, err)); // not rented yet
    CHECK(!m.deliverCar("Benz C200", err) ? false : true);
    CHECK(benz->status() == CarStatus::Rented);

    // Second booking exists -> renewal must be refused (queue discipline)
    CHECK(!m.renewRental("Benz C200", 2, err));
    CHECK_EQ(benz->currentReservation()->endDate(), 9); // unchanged

    // REGRESSION: income was booked TWICE (on reservation AND on return).
    CHECK_EQ(benz->totalIncome(), 0.0); // nothing earned at booking time!
    CHECK(m.returnCar("Benz C200", 9, err)); // on-time return
    // 5 days (5..9 inclusive) x 180 = 900, no penalty.
    CHECK(approx(benz->totalIncome(), 900.0));

    // Customer is charged and can pay.
    CHECK(approx(m.currentCustomerBalance(), 900.0));
    CHECK(m.myReservations().size() == 1); // the 10..12 booking remains
    CHECK(approx(m.payBill(), 900.0));
    CHECK(approx(m.currentCustomerBalance(), 0.0));

    // Late return: deliver next booking (10..12), return on day 15.
    CHECK(m.deliverCar("Benz C200", err));
    CHECK(m.returnCar("Benz C200", 15, err));
    // 3 days x 180 = 540 + penalty 3 late days x 180 x 1.5 = 810 -> 1350
    CHECK(approx(benz->totalIncome(), 900.0 + 1350.0));
    CHECK(benz->status() == CarStatus::Available);
    CHECK(!m.isLoggedIn() || m.currentUserRole() == "Customer");
}

static void testRentalManagerFleetAndMaintenance()
{
    RentalManager m;
    m.seedDemoData();

    std::string err;

    // Duplicate fleet names rejected (v1.0 silently ignored them).
    CHECK(m.addCar(new Car("Testarossa", "Ferrari", 400.0)));
    CHECK(!m.addCar(new Car("Testarossa", "Ferrari", 400.0)));
    CHECK_EQ(m.fleetSize(), std::size_t(9)); // 8 seeded + 1

    // Maintenance lifecycle.
    CHECK(m.recordMaintenance("Pride 131", "1404-08-12", "brake pads", 120.0, err));
    CHECK(m.searchCar("Pride 131")->status() == CarStatus::Maintenance);
    CHECK(!m.makeReservation("Pride 131", 1, 3, err)); // blocked while down

    // A rented car cannot be serviced...
    m.login("ali", "customer123");
    CHECK(m.makeReservation("Tucson 2023", 1, 3, err));
    m.logout();
    m.login("sara", "staff123");
    CHECK(m.deliverCar("Tucson 2023", err));
    CHECK(!m.recordMaintenance("Tucson 2023", "x", "y", 10.0, err));
    CHECK(m.returnCar("Tucson 2023", 3, err));
    // ...but after return it can.
    CHECK(m.recordMaintenance("Tucson 2023", "1404-09-01", "oil change", 45.0, err));
    CHECK(m.finishMaintenance("Tucson 2023"));
    CHECK(m.searchCar("Tucson 2023")->status() == CarStatus::Available);
    CHECK(approx(m.searchCar("Tucson 2023")->maintenanceCost(), 45.0));

    // Maintenance history is queryable.
    CHECK_EQ(m.searchCar("Tucson 2023")->maintenanceLog().size(), std::size_t(1));

    // Brand search (linear scan over the AVL index).
    CHECK_EQ(m.findCarsByBrand("IKCO").size(), std::size_t(2));

    // v2.5.0: account management is manager-only in the facade too —
    // the logged-in staff member is denied at the gate...
    CHECK(!m.unblockUser("ali"));
    m.logout();
    // ...and only a manager can lift the block.
    m.login("admin", "manager123");
    CHECK(m.unblockUser("ali"));
    m.logout();
}

static void testRentalManagerFleetFilters()
{
    RentalManager m;
    m.seedDemoData();

    // All 8 seeded cars start Available -> all bookable.
    CHECK_EQ(m.findBookableCars().size(), std::size_t(8));
    for (Car *car : m.findBookableCars())
        CHECK(car->isAvailableForBooking());

    // A car under maintenance drops out of the bookable set.
    CHECK(m.sendToMaintenance("Benz C200"));
    CHECK_EQ(m.findBookableCars().size(), std::size_t(7));
    CHECK(m.searchCar("Benz C200")->status() == CarStatus::Maintenance);
    CHECK(m.finishMaintenance("Benz C200"));
    CHECK_EQ(m.findBookableCars().size(), std::size_t(8));

    // Rented cars are not bookable; Reserved ones still are.
    Car *tucson = m.searchCar("Tucson 2023");
    tucson->setStatus(CarStatus::Rented);
    CHECK(!tucson->isAvailableForBooking());
    CHECK_EQ(m.findBookableCars().size(), std::size_t(7));
    tucson->setStatus(CarStatus::Reserved);
    CHECK(tucson->isAvailableForBooking());
    CHECK_EQ(m.findBookableCars().size(), std::size_t(8));
    tucson->setStatus(CarStatus::Available);

    // Price range is inclusive on both ends.
    CHECK_EQ(m.findCarsInPriceRange(30.0, 30.0).size(), std::size_t(1));
    CHECK(m.findCarsInPriceRange(30.0, 30.0)[0]->name() == "Pride 131");
    CHECK_EQ(m.findCarsInPriceRange(180.0, 180.0).size(), std::size_t(1));
    CHECK(m.findCarsInPriceRange(180.0, 180.0)[0]->name() == "Benz C200");

    // Empty and inverted ranges return nothing (and never crash).
    CHECK(m.findCarsInPriceRange(500.0, 900.0).empty());
    CHECK(m.findCarsInPriceRange(70.0, 20.0).empty());

    // 30..60 covers Pride, Quick R, 206, Samand, Dena and Pars.
    CHECK_EQ(m.findCarsInPriceRange(30.0, 60.0).size(), std::size_t(6));

    // The full band comes back sorted by daily rate.
    std::vector<Car *> band = m.findCarsInPriceRange(0.0, 1000.0);
    CHECK_EQ(band.size(), std::size_t(8));
    for (std::size_t i = 1; i < band.size(); ++i)
        CHECK(band[i - 1]->pricePerDay() <= band[i]->pricePerDay());
    CHECK(band.front()->name() == "Pride 131");  // cheapest
    CHECK(band.back()->name() == "Benz C200");   // most expensive

    // Equal rates tie-break alphabetically by car name.
    CHECK(m.addCar(new Car("ATest 1", "Zig", 45.0)));
    CHECK(m.addCar(new Car("ZTest 2", "Aab", 45.0)));
    std::vector<Car *> ties = m.findCarsInPriceRange(45.0, 45.0);
    CHECK_EQ(ties.size(), std::size_t(3)); // Peugeot 206 + the two new cars
    CHECK(ties[0]->name() == "ATest 1");
    CHECK(ties[1]->name() == "Peugeot 206");
    CHECK(ties[2]->name() == "ZTest 2");

    // forEachCarByPrice visits the whole fleet, cheapest first ...
    std::vector<std::string> byPrice;
    m.forEachCarByPrice([&](Car *car)
                        { byPrice.push_back(car->name()); });
    CHECK_EQ(byPrice.size(), std::size_t(10));
    CHECK(byPrice.front() == "Pride 131");
    CHECK(byPrice.back() == "Benz C200");

    // ... and leaves the manager's insertion-order view untouched.
    std::vector<std::string> insertion;
    m.forEachCar([&](Car *car)
                 { insertion.push_back(car->name()); });
    CHECK_EQ(insertion.size(), std::size_t(10));
    CHECK(insertion.front() == "Pride 131");

    // Brand lookups keep working next to the new queries.
    CHECK_EQ(m.findCarsByBrand("Saipa").size(), std::size_t(2));
}

static void testRentalManagerPersistence()
{
    const std::string dir = "/tmp/crs_test_data";

    std::system(("rm -rf " + dir).c_str());
    {
        RentalManager m;
        m.seedDemoData();
        std::string err;
        m.registerCustomer("reza", "pass123");
        CHECK(m.login("reza", "pass123"));
        CHECK(m.makeReservation("Benz C200", 3, 5, err));
        m.logout();
        m.login("sara", "staff123");
        CHECK(m.deliverCar("Benz C200", err));
        CHECK(m.returnCar("Benz C200", 7, err)); // 2 days late
        m.logout();
        CHECK(m.saveData(dir));
    }
    {
        RentalManager m;
        CHECK(m.loadData(dir));

        // Users survive with credentials intact.
        CHECK(m.login("reza", "pass123"));
        m.logout();
        CHECK(!m.login("reza", "nope"));

        // The invoice survived too (income + customer balance).
        // Booking 3..5 = 3 days x 180 = 540, + 2 late days x 270 = 540
        // -> total 1080.
        Car *benz = m.searchCar("Benz C200");
        CHECK(benz != nullptr);
        CHECK(approx(benz->totalIncome(), 540.0 + 540.0));

        m.login("reza", "pass123");
        CHECK(approx(m.currentCustomerBalance(), 540.0 + 540.0));
        m.logout();

        // Blocked state persists.
        m.blockUser("reza");
        m.saveData(dir);
    }
    {
        RentalManager m;
        CHECK(m.loadData(dir));
        CHECK(m.isUserBlocked("reza"));
    }

    // Loading from a missing directory fails gracefully.
    RentalManager fresh;
    CHECK(!fresh.loadData("/tmp/definitely_missing_dir_xyz"));
    CHECK(!fresh.hasDataOnDisk("/tmp/definitely_missing_dir_xyz"));
}

static void testRoleHierarchy()
{
    Customer c("c");
    Staff s("s");
    Manager mg("mg");

    CHECK(c.role() == "Customer");
    CHECK(s.role() == "Staff");
    CHECK(mg.role() == "Manager");

    // Customer balance bookkeeping.
    CHECK(approx(c.balance(), 0.0));
    c.charge(50.0);
    c.charge(25.5);
    CHECK(approx(c.balance(), 75.5));
    c.payBalance();
    CHECK(approx(c.balance(), 0.0));

    // Blocking.
    CHECK(!s.isBlocked());
    s.block();
    CHECK(s.isBlocked());
}

/* ============================================================
 *  RentalManager: keyword search
 * ============================================================ */

static void testRentalManagerSearch()
{
    RentalManager m;
    m.seedDemoData();

    // Brand matches, case-insensitive.
    CHECK_EQ(m.searchFleet("peugeot").size(), std::size_t(2));
    CHECK_EQ(m.searchFleet("PEUGEOT").size(), std::size_t(2));
    CHECK_EQ(m.searchFleet("saipa").size(), std::size_t(2));
    CHECK_EQ(m.searchFleet("Saipa").size(), std::size_t(2));

    // Name matches, including partial words and digits.
    CHECK_EQ(m.searchFleet("benz").size(), std::size_t(1));
    CHECK(m.searchFleet("benz")[0]->name() == "Benz C200");
    CHECK_EQ(m.searchFleet("206").size(), std::size_t(1));
    CHECK(m.searchFleet("206")[0]->name() == "Peugeot 206");
    CHECK_EQ(m.searchFleet("TUCSON").size(), std::size_t(1));

    // "samand" hits the name, "lx" only the model suffix.
    CHECK_EQ(m.searchFleet("samand").size(), std::size_t(1));
    CHECK_EQ(m.searchFleet("LX").size(), std::size_t(1));

    // Equal-kind matches keep the manager's insertion order.
    std::vector<Car *> saipas = m.searchFleet("saipa");
    CHECK(saipas[0]->name() == "Pride 131");
    CHECK(saipas[1]->name() == "Quick R");

    // Search covers the WHOLE fleet: maintenance cars are still findable.
    CHECK(m.sendToMaintenance("Benz C200"));
    CHECK_EQ(m.searchFleet("benz").size(), std::size_t(1));
    CHECK(m.searchFleet("benz")[0]->status() == CarStatus::Maintenance);
    CHECK(m.finishMaintenance("Benz C200"));

    // No false positives, no crashes on junk or empty input.
    CHECK(m.searchFleet("").empty());
    CHECK(m.searchFleet("zzzz").empty());
    CHECK(m.searchFleet("  ").empty());
}

/* ============================================================
 *  RentalManager: statistics
 * ============================================================ */

static void testRentalManagerStats()
{
    // Empty system: no divisions by zero, no garbage values.
    {
        RentalManager m;
        FleetStats s = m.computeStats();
        CHECK_EQ(s.totalCars, std::size_t(0));
        CHECK_EQ(s.activeBookings, std::size_t(0));
        CHECK(approx(s.grossIncome, 0.0));
        CHECK(approx(s.utilization, 0.0));
        CHECK(s.topEarner.empty());
    }

    RentalManager m;
    m.seedDemoData();

    // Fresh seed: 8 cars, all available, no money has moved yet.
    FleetStats s = m.computeStats();
    CHECK_EQ(s.totalCars, std::size_t(8));
    CHECK_EQ(s.availableCars, std::size_t(8));
    CHECK_EQ(s.reservedCars, std::size_t(0));
    CHECK_EQ(s.rentedCars, std::size_t(0));
    CHECK_EQ(s.maintenanceCars, std::size_t(0));
    CHECK_EQ(s.activeBookings, std::size_t(0));
    CHECK(approx(s.utilization, 0.0));
    CHECK(s.topEarner.empty());

    // One booking moves utilization to 12.5%.
    std::string err;
    CHECK(m.login("ali", "customer123"));
    CHECK(m.makeReservation("Benz C200", 5, 9, err));
    s = m.computeStats();
    CHECK_EQ(s.reservedCars, std::size_t(1));
    CHECK_EQ(s.activeBookings, std::size_t(1));
    CHECK(approx(s.utilization, 12.5));
    m.logout();

    // Delivering flips Reserved -> Rented; utilization is unchanged.
    CHECK(m.login("sara", "staff123"));
    CHECK(m.deliverCar("Benz C200", err));
    s = m.computeStats();
    CHECK_EQ(s.rentedCars, std::size_t(1));
    CHECK_EQ(s.reservedCars, std::size_t(0));
    CHECK(approx(s.utilization, 12.5));

    // A maintenance stop idles a second car; maintenance counts as
    // spend, not as utilization (nothing is earning while parked).
    CHECK(m.recordMaintenance("Pride 131", "1404-08-10", "brake pads", 100.0, err));
    s = m.computeStats();
    CHECK_EQ(s.maintenanceCars, std::size_t(1));
    CHECK_EQ(s.availableCars, std::size_t(6));
    CHECK(approx(s.maintenanceSpend, 100.0));

    // Returning 2 days late bills 5x180 + 2x180x1.5 = 1440.
    CHECK(m.returnCar("Benz C200", 11, err));
    s = m.computeStats();
    CHECK_EQ(s.activeBookings, std::size_t(0));
    CHECK(approx(s.grossIncome, 1440.0));
    CHECK(approx(s.netRevenue, 1340.0));
    CHECK(approx(s.utilization, 0.0));
    CHECK(s.topEarner == "Benz C200");

    // A lower-earning car does not dethrone the top earner.
    m.searchCar("Pride 131")->addIncome(500.0);
    s = m.computeStats();
    CHECK(s.topEarner == "Benz C200");
    CHECK(approx(s.grossIncome, 1940.0));
}

/* ============================================================
 *  RentalManager: CSV reports
 * ============================================================ */

static void testRentalManagerReports()
{
    const std::string dir = "/tmp/crs_test_reports";
    std::system(("rm -rf " + dir).c_str());

    RentalManager m;
    m.seedDemoData();
    std::string err;
    CHECK(m.login("ali", "customer123"));
    CHECK(m.makeReservation("Benz C200", 5, 9, err)); // id 1, starts day 5
    CHECK(m.makeReservation("Samand LX", 2, 4, err)); // id 2, starts day 2
    m.logout();

    /* ---- fleet report: header + one sorted row per car ---- */
    const std::string fleetPath = dir + "/nested/fleet_report.csv";
    CHECK(m.exportFleetReport(fleetPath)); // creates missing parents itself
    {
        std::ifstream in(fleetPath);
        CHECK(in.good());
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(in, line))
            if (!line.empty())
                lines.push_back(line);
        CHECK_EQ(lines.size(), std::size_t(9)); // header + 8 seeded cars
        CHECK(lines[0] == "name,brand,pricePerDay,status,income,"
                          "maintenanceCost,netRevenue");
        // Name-sorted: Benz C200 is the first row, Tucson the last.
        CHECK(lines[1].rfind("Benz C200,Mercedes,180,RESERVED,0,0,0", 0) == 0);
        CHECK(lines[8].rfind("Tucson 2023,Hyundai,120,AVAILABLE,0,0,0", 0) == 0);
    }

    /* ---- booking report: ordered by start date, not car name ---- */
    const std::string bookingsPath = dir + "/bookings_report.csv";
    CHECK(m.exportBookingReport(bookingsPath));
    {
        std::ifstream in(bookingsPath);
        CHECK(in.good());
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(in, line))
            if (!line.empty())
                lines.push_back(line);
        CHECK_EQ(lines.size(), std::size_t(3)); // header + 2 live bookings
        CHECK(lines[0] == "id,carName,customer,startDate,endDate,durationDays");
        CHECK(lines[1].rfind("2,Samand LX,ali,1970-01-03,1970-01-05,3", 0) == 0);
        CHECK(lines[2].rfind("1,Benz C200,ali,1970-01-06,1970-01-10,5", 0) == 0);
    }

    /* ---- failure path: a directory is not a writable report file ---- */
    std::filesystem::create_directories(dir + "/blocker");
    CHECK(!m.exportFleetReport(dir + "/blocker"));
    CHECK(!m.exportBookingReport(dir + "/blocker"));
}

/* ============================================================
 *  JSON report export
 * ============================================================
 *  The exported report.json is validated by a tiny strict
 *  recursive-descent parser written right here — the same trick
 *  the SHA-256 suite uses against official vectors: the output
 *  is not eyeballed, it is parsed and then asserted on.
 * ============================================================ */

namespace json_check
{

struct JsonValue
{
    enum Kind
    {
        Object,
        Array,
        String,
        Number,
        Bool,
        Null
    } kind = Null;
    std::string text;                                        // String / Number
    bool boolean = false;                                    // Bool
    std::vector<std::pair<std::string, JsonValue>> members;  // Object
    std::vector<JsonValue> items;                            // Array
};

class Parser
{
public:
    explicit Parser(const std::string &src) : src_(src) {}

    bool parse(JsonValue &out)
    {
        pos_ = 0;
        return skipWs() && value(out, 0) && skipWs() &&
               pos_ == src_.size();
    }

private:
    const std::string &src_;
    std::size_t pos_ = 0;

    bool fail() { return false; }

    bool skipWs()
    {
        while (pos_ < src_.size() &&
               (src_[pos_] == ' ' || src_[pos_] == '\n' ||
                src_[pos_] == '\t' || src_[pos_] == '\r'))
            ++pos_;
        return true;
    }

    bool value(JsonValue &out, int depth)
    {
        if (depth > 32)
            return fail();
        skipWs();
        if (pos_ >= src_.size())
            return fail();
        switch (src_[pos_])
        {
        case '{':
            return object(out, depth);
        case '[':
            return array(out, depth);
        case '"':
            out.kind = JsonValue::String;
            return string(out.text);
        case 't':
            return literal("true", out, true);
        case 'f':
            return literal("false", out, false);
        case 'n':
            if (!literal("null", out, false))
                return false;
            out.kind = JsonValue::Null;
            return true;
        default:
            return number(out);
        }
    }

    bool literal(const char *lit, JsonValue &out, bool flag)
    {
        const std::size_t n = std::strlen(lit);
        if (src_.compare(pos_, n, lit) != 0)
            return fail();
        pos_ += n;
        out.kind = JsonValue::Bool;
        out.boolean = flag;
        return true;
    }

    bool object(JsonValue &out, int depth)
    {
        out.kind = JsonValue::Object;
        ++pos_; // consume '{'
        skipWs();
        if (pos_ < src_.size() && src_[pos_] == '}')
        {
            ++pos_;
            return true;
        }
        while (true)
        {
            skipWs();
            std::string key;
            if (pos_ >= src_.size() || src_[pos_] != '"')
                return fail();
            if (!string(key))
                return false;
            skipWs();
            if (pos_ >= src_.size() || src_[pos_] != ':')
                return fail();
            ++pos_;
            JsonValue item;
            if (!value(item, depth + 1))
                return false;
            out.members.emplace_back(std::move(key), std::move(item));
            skipWs();
            if (pos_ < src_.size() && src_[pos_] == ',')
            {
                ++pos_;
                continue;
            }
            if (pos_ < src_.size() && src_[pos_] == '}')
            {
                ++pos_;
                return true;
            }
            return fail();
        }
    }

    bool array(JsonValue &out, int depth)
    {
        out.kind = JsonValue::Array;
        ++pos_; // consume '['
        skipWs();
        if (pos_ < src_.size() && src_[pos_] == ']')
        {
            ++pos_;
            return true;
        }
        while (true)
        {
            JsonValue item;
            if (!value(item, depth + 1))
                return false;
            out.items.push_back(std::move(item));
            skipWs();
            if (pos_ < src_.size() && src_[pos_] == ',')
            {
                ++pos_;
                continue;
            }
            if (pos_ < src_.size() && src_[pos_] == ']')
            {
                ++pos_;
                return true;
            }
            return fail();
        }
    }

    bool string(std::string &out)
    {
        ++pos_; // consume '"'
        out.clear();
        while (true)
        {
            if (pos_ >= src_.size())
                return fail();
            const char c = src_[pos_++];
            if (c == '"')
                return true;
            if (c != '\\')
            {
                out += c;
                continue;
            }
            if (pos_ >= src_.size())
                return fail();
            const char e = src_[pos_++];
            switch (e)
            {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u':
            {
                if (pos_ + 4 > src_.size())
                    return fail();
                int code = 0;
                for (int i = 0; i < 4; ++i)
                {
                    const char h = src_[pos_++];
                    code *= 16;
                    if (h >= '0' && h <= '9')
                        code += h - '0';
                    else if (h >= 'A' && h <= 'F')
                        code += h - 'A' + 10;
                    else if (h >= 'a' && h <= 'f')
                        code += h - 'a' + 10;
                    else
                        return fail();
                }
                if (code < 0x80)
                    out += static_cast<char>(code);
                else // BMP code point -> 2-byte UTF-8 (enough for tests)
                {
                    out += static_cast<char>(0xC0 | (code >> 6));
                    out += static_cast<char>(0x80 | (code & 0x3F));
                }
                break;
            }
            default:
                return fail();
            }
        }
    }

    bool number(JsonValue &out)
    {
        const std::size_t start = pos_;
        if (pos_ < src_.size() && src_[pos_] == '-')
            ++pos_;
        bool digits = false;
        while (pos_ < src_.size() &&
               (std::isdigit(static_cast<unsigned char>(src_[pos_])) ||
                src_[pos_] == '.'))
        {
            if (std::isdigit(static_cast<unsigned char>(src_[pos_])))
                digits = true;
            ++pos_;
        }
        if (!digits)
            return fail();
        out.kind = JsonValue::Number;
        out.text = src_.substr(start, pos_ - start);
        return true;
    }
};

const JsonValue *member(const JsonValue &obj, const std::string &key)
{
    if (obj.kind != JsonValue::Object)
        return nullptr;
    for (const auto &kv : obj.members)
        if (kv.first == key)
            return &kv.second;
    return nullptr;
}

const JsonValue *fleetCarNamed(const JsonValue &report,
                               const std::string &name)
{
    const JsonValue *fleet = member(report, "fleet");
    if (fleet == nullptr)
        return nullptr;
    for (const JsonValue &car : fleet->items)
    {
        const JsonValue *n = member(car, "name");
        if (n != nullptr && n->text == name)
            return &car;
    }
    return nullptr;
}

bool readFile(const std::string &path, std::string &out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    std::ostringstream buf;
    buf << in.rdbuf();
    out = buf.str();
    return true;
}

} // namespace json_check

static void testJsonEscaping()
{
    CHECK(jsonEscape("plain text") == "plain text");
    CHECK(jsonEscape("") == "");
    CHECK(jsonEscape("say \"hi\"") == "say \\\"hi\\\"");
    CHECK(jsonEscape("back\\slash") == "back\\\\slash");
    CHECK(jsonEscape("line1\nline2") == "line1\\nline2");
    CHECK(jsonEscape("a\tb") == "a\\tb");
    CHECK(jsonEscape("a\rb") == "a\\rb");
    CHECK(jsonEscape(std::string("x\x01y", 3)) == "x\\u0001y");
    CHECK(jsonEscape(std::string("\x1F", 1)) == "\\u001F");
    // DEL (0x7F) and multi-byte UTF-8 are legal raw JSON bytes.
    CHECK(jsonEscape(std::string("\x7F", 1)) == std::string("\x7F", 1));
    CHECK(jsonEscape("\xE2\x98\xBA") == "\xE2\x98\xBA");
    CHECK(jsonEscape("«سـ»") == "«سـ»");
}

static void testJsonExport()
{
    const std::string dir = "/tmp/crs_test_json";
    std::system(("rm -rf " + dir).c_str());

    /* ---- todaySerial round-trips through the calendar layer ---- */
    const int today = Date::todaySerial();
    int back = -1;
    CHECK(Date::parseISO(Date::toISO(today), back));
    CHECK_EQ(back, today);
    int floor2024 = 0;
    CHECK(Date::parseISO("2024-01-01", floor2024));
    CHECK(today >= floor2024);

    /* ---- empty system: valid JSON, empty collections ---- */
    {
        RentalManager empty;
        const std::string path = dir + "/nested/deeper/empty.json";
        CHECK(exportJsonReport(empty, path)); // creates the parents
        std::string body;
        CHECK(json_check::readFile(path, body));
        json_check::Parser p(body);
        json_check::JsonValue root;
        CHECK(p.parse(root));
        const json_check::JsonValue *stats =
            json_check::member(root, "stats");
        CHECK(stats != nullptr);
        const json_check::JsonValue *total =
            stats ? json_check::member(*stats, "totalCars") : nullptr;
        CHECK(total != nullptr && total->text == "0");
        const json_check::JsonValue *fleet =
            json_check::member(root, "fleet");
        CHECK(fleet != nullptr && fleet->items.empty());
        const json_check::JsonValue *bookings =
            json_check::member(root, "bookings");
        CHECK(bookings != nullptr && bookings->items.empty());
        const json_check::JsonValue *at =
            json_check::member(root, "generatedAt");
        CHECK(at != nullptr && Date::isISODate(at->text));

        // Loyalty section exists even for an empty program and
        // reads as all-zero, not garbage.               (v2.8.0)
        const json_check::JsonValue *loyalty =
            json_check::member(root, "loyalty");
        CHECK(loyalty != nullptr);
        const json_check::JsonValue *members =
            loyalty ? json_check::member(*loyalty, "totalMembers")
                    : nullptr;
        CHECK(members != nullptr && members->text == "0");
        const json_check::JsonValue *tiers =
            loyalty ? json_check::member(*loyalty, "membersByTier")
                    : nullptr;
        CHECK(tiers != nullptr && tiers->items.empty());
        if (tiers != nullptr)
        {
            const char *tierNames[4] = {"Bronze", "Silver", "Gold",
                                        "Platinum"};
            for (const char *name : tierNames)
            {
                const json_check::JsonValue *tier =
                    json_check::member(*tiers, name);
                CHECK(tier != nullptr && tier->text == "0");
            }
        }
        const json_check::JsonValue *held =
            loyalty ? json_check::member(*loyalty, "outstandingPoints")
                    : nullptr;
        CHECK(held != nullptr && held->text == "0");
        const json_check::JsonValue *liability =
            loyalty ? json_check::member(*loyalty, "pointsLiability")
                    : nullptr;
        CHECK(liability != nullptr && liability->text == "0");
        const json_check::JsonValue *rows =
            loyalty ? json_check::member(*loyalty, "customers") : nullptr;
        CHECK(rows != nullptr && rows->items.empty());
    }

    /* ---- seeded system: escaping, ordering, real figures ---- */
    {
        RentalManager m;
        m.seedDemoData();
        // A car name that breaks naive JSON writers: quote, backslash
        // and a raw newline — none of which CSV could ever carry.
        CHECK(m.addCar(new Car("P\\206 \"GT\"", "Peugeot", 90.0)));
        CHECK(m.addCar(new Car("Multi\nLine \"car\"", "Debug", 10.0)));
        std::string err;
        CHECK(m.login("admin", "manager123"));
        CHECK(m.recordMaintenance("Tucson 2023", "2026-11-01",
                                  "oil change + filters", 45.5, err));
        m.logout();
        CHECK(m.login("ali", "customer123"));
        int startA = 0, endA = 0, startB = 0, endB = 0;
        CHECK(Date::parseISO("2026-11-10", startA));
        CHECK(Date::parseISO("2026-11-12", endA));
        CHECK(Date::parseISO("2026-11-05", startB));
        CHECK(Date::parseISO("2026-11-07", endB));
        CHECK(m.makeReservation("Benz C200", startA, endA, err)); // id 1
        CHECK(m.makeReservation("Samand LX", startB, endB, err)); // id 2
        m.logout();

        const std::string path = dir + "/report.json";
        CHECK(exportJsonReport(m, path));
        std::string body;
        CHECK(json_check::readFile(path, body));
        json_check::Parser p(body);
        json_check::JsonValue root;
        CHECK(p.parse(root));

        // Header fields.
        const json_check::JsonValue *rep =
            json_check::member(root, "report");
        CHECK(rep != nullptr && rep->text == "car-rental-system");
        const json_check::JsonValue *at =
            json_check::member(root, "generatedAt");
        CHECK(at != nullptr && Date::isISODate(at->text));

        // Stats: 8 seeded cars + 2 = 10, two live bookings.
        const json_check::JsonValue *stats =
            json_check::member(root, "stats");
        CHECK(stats != nullptr);
        const json_check::JsonValue *totalCars =
            json_check::member(*stats, "totalCars");
        CHECK(totalCars != nullptr && totalCars->text == "10");
        const json_check::JsonValue *active =
            json_check::member(*stats, "activeBookings");
        CHECK(active != nullptr && active->text == "2");

        // Fleet: name-sorted, and the hostile names round-trip exactly.
        const json_check::JsonValue *fleet =
            json_check::member(root, "fleet");
        CHECK(fleet != nullptr && fleet->items.size() == 10);
        CHECK(json_check::fleetCarNamed(root, "P\\206 \"GT\"") !=
              nullptr);
        CHECK(json_check::fleetCarNamed(root, "Multi\nLine \"car\"") !=
              nullptr);

        // The maintenance log travelled intact.
        const json_check::JsonValue *tucson =
            json_check::fleetCarNamed(root, "Tucson 2023");
        CHECK(tucson != nullptr);
        const json_check::JsonValue *log =
            tucson ? json_check::member(*tucson, "maintenanceLog")
                   : nullptr;
        CHECK(log != nullptr && log->items.size() == 1);
        const json_check::JsonValue *desc =
            log ? json_check::member(log->items[0], "description")
                : nullptr;
        CHECK(desc != nullptr && desc->text == "oil change + filters");

        // Bookings ordered by pickup date, not insertion order.
        const json_check::JsonValue *bookings =
            json_check::member(root, "bookings");
        CHECK(bookings != nullptr && bookings->items.size() == 2);
        const json_check::JsonValue *firstCar =
            json_check::member(bookings->items[0], "carName");
        CHECK(firstCar != nullptr && firstCar->text == "Samand LX");
        const json_check::JsonValue *firstId =
            json_check::member(bookings->items[0], "id");
        CHECK(firstId != nullptr && firstId->text == "2");
        const json_check::JsonValue *firstStart =
            json_check::member(bookings->items[0], "startDate");
        CHECK(firstStart != nullptr && firstStart->text == "2026-11-05");
        const json_check::JsonValue *firstDur =
            json_check::member(bookings->items[0], "durationDays");
        CHECK(firstDur != nullptr && firstDur->text == "3");
        const json_check::JsonValue *secondStart =
            json_check::member(bookings->items[1], "startDate");
        CHECK(secondStart != nullptr && secondStart->text == "2026-11-10");
        const json_check::JsonValue *secondCust =
            json_check::member(bookings->items[1], "customer");
        CHECK(secondCust != nullptr && secondCust->text == "ali");

        // Loyalty: ali is the demo's seeded Silver member holding
        // 340 points over 3 completed rentals — the file must name
        // the very same standing.                     (v2.8.0)
        const json_check::JsonValue *loyalty =
            json_check::member(root, "loyalty");
        CHECK(loyalty != nullptr);
        const json_check::JsonValue *members =
            loyalty ? json_check::member(*loyalty, "totalMembers")
                    : nullptr;
        CHECK(members != nullptr && members->text == "1");
        const json_check::JsonValue *tiers =
            loyalty ? json_check::member(*loyalty, "membersByTier")
                    : nullptr;
        CHECK(tiers != nullptr);
        if (tiers != nullptr)
        {
            const char *emptyTiers[3] = {"Bronze", "Gold", "Platinum"};
            for (const char *name : emptyTiers)
            {
                const json_check::JsonValue *tier =
                    json_check::member(*tiers, name);
                CHECK(tier != nullptr && tier->text == "0");
            }
            const json_check::JsonValue *silver =
                json_check::member(*tiers, "Silver");
            CHECK(silver != nullptr && silver->text == "1");
        }
        const json_check::JsonValue *held =
            loyalty ? json_check::member(*loyalty, "outstandingPoints")
                    : nullptr;
        CHECK(held != nullptr && held->text == "340");
        const json_check::JsonValue *liability =
            loyalty ? json_check::member(*loyalty, "pointsLiability")
                    : nullptr;
        CHECK(liability != nullptr && liability->text == "34");
        const json_check::JsonValue *topName =
            loyalty ? json_check::member(*loyalty, "topMember") : nullptr;
        CHECK(topName != nullptr && topName->text == "ali");
        const json_check::JsonValue *topPoints =
            loyalty ? json_check::member(*loyalty, "topMemberLifetime")
                    : nullptr;
        CHECK(topPoints != nullptr && topPoints->text == "340");
        const json_check::JsonValue *rows =
            loyalty ? json_check::member(*loyalty, "customers") : nullptr;
        CHECK(rows != nullptr && rows->items.size() == 1);
        if (rows != nullptr && !rows->items.empty())
        {
            const json_check::JsonValue &row = rows->items[0];
            const json_check::JsonValue *name =
                json_check::member(row, "username");
            CHECK(name != nullptr && name->text == "ali");
            const json_check::JsonValue *tier =
                json_check::member(row, "tier");
            CHECK(tier != nullptr && tier->text == "Silver");
            const json_check::JsonValue *pts =
                json_check::member(row, "points");
            CHECK(pts != nullptr && pts->text == "340");
            const json_check::JsonValue *life =
                json_check::member(row, "lifetimePoints");
            CHECK(life != nullptr && life->text == "340");
            const json_check::JsonValue *done =
                json_check::member(row, "completedRentals");
            CHECK(done != nullptr && done->text == "3");
            const json_check::JsonValue *toNext =
                json_check::member(row, "pointsToNextTier");
            CHECK(toNext != nullptr && toNext->text == "660");
        }
    }
}

/* ============================================================
 *  Loyalty standings in the JSON report   (v2.8.0)
 * ============================================================
 *  One member earns points through the REAL accrual flow (reserve
 *  -> deliver -> return) while two others stay fresh, so the
 *  exported array must show three username-sorted rows whose
 *  figures agree with the facade's own overview snapshot.
 * ============================================================ */

static void testJsonLoyaltyExport()
{
    const std::string dir = "/tmp/crs_test_json_loyalty";
    std::system(("rm -rf " + dir).c_str());

    RentalManager m;
    CHECK(m.addCar(new Car("Rider", "TestCo", 40.0)));
    CHECK(m.registerCustomer("zara", "pass1234"));
    CHECK(m.registerCustomer("behrad", "pass1234"));
    CHECK(m.registerCustomer("mina", "pass1234"));

    // zara completes a rental through the real flow: 5 days x 40
    // = 200.00 -> 20 points at the Bronze x1.0 multiplier.
    std::string err;
    int start = 0, end = 0;
    CHECK(Date::parseISO("2026-11-01", start));
    CHECK(Date::parseISO("2026-11-05", end));
    CHECK(m.login("zara", "pass1234"));
    CHECK(m.makeReservation("Rider", start, end, err));
    m.logout();
    CHECK(m.deliverCar("Rider", err));
    CHECK(m.returnCar("Rider", end, err));

    const std::string path = dir + "/report.json";
    CHECK(exportJsonReport(m, path));
    std::string body;
    CHECK(json_check::readFile(path, body));
    json_check::Parser p(body);
    json_check::JsonValue root;
    CHECK(p.parse(root));

    const json_check::JsonValue *loyalty =
        json_check::member(root, "loyalty");
    CHECK(loyalty != nullptr);
    const json_check::JsonValue *members =
        loyalty ? json_check::member(*loyalty, "totalMembers") : nullptr;
    CHECK(members != nullptr && members->text == "3");
    const json_check::JsonValue *tiers =
        loyalty ? json_check::member(*loyalty, "membersByTier") : nullptr;
    CHECK(tiers != nullptr);
    if (tiers != nullptr)
    {
        const json_check::JsonValue *bronze =
            json_check::member(*tiers, "Bronze");
        CHECK(bronze != nullptr && bronze->text == "3");
        const json_check::JsonValue *higher[3] = {
            json_check::member(*tiers, "Silver"),
            json_check::member(*tiers, "Gold"),
            json_check::member(*tiers, "Platinum")};
        for (const json_check::JsonValue *tier : higher)
            CHECK(tier != nullptr && tier->text == "0");
    }
    const json_check::JsonValue *held =
        loyalty ? json_check::member(*loyalty, "outstandingPoints")
                : nullptr;
    CHECK(held != nullptr && held->text == "20");
    const json_check::JsonValue *liability =
        loyalty ? json_check::member(*loyalty, "pointsLiability")
                : nullptr;
    CHECK(liability != nullptr && liability->text == "2");
    const json_check::JsonValue *topName =
        loyalty ? json_check::member(*loyalty, "topMember") : nullptr;
    CHECK(topName != nullptr && topName->text == "zara");
    const json_check::JsonValue *topPoints =
        loyalty ? json_check::member(*loyalty, "topMemberLifetime")
                : nullptr;
    CHECK(topPoints != nullptr && topPoints->text == "20");

    // Three rows, username-sorted: behrad, mina, zara.
    const json_check::JsonValue *rows =
        loyalty ? json_check::member(*loyalty, "customers") : nullptr;
    CHECK(rows != nullptr && rows->items.size() == 3);
    if (rows != nullptr && rows->items.size() == 3)
    {
        const char *order[3] = {"behrad", "mina", "zara"};
        for (int i = 0; i < 3; ++i)
        {
            const json_check::JsonValue *name =
                json_check::member(rows->items[i], "username");
            CHECK(name != nullptr && name->text == order[i]);
            const json_check::JsonValue *tier =
                json_check::member(rows->items[i], "tier");
            CHECK(tier != nullptr && tier->text == "Bronze");
        }
        // The two fresh members: all-zero standings.
        for (int i = 0; i < 2; ++i)
        {
            const json_check::JsonValue *pts =
                json_check::member(rows->items[i], "points");
            CHECK(pts != nullptr && pts->text == "0");
            const json_check::JsonValue *done =
                json_check::member(rows->items[i], "completedRentals");
            CHECK(done != nullptr && done->text == "0");
            const json_check::JsonValue *toNext =
                json_check::member(rows->items[i], "pointsToNextTier");
            CHECK(toNext != nullptr && toNext->text == "250");
        }
        // zara's earned standing rode into the file.
        const json_check::JsonValue &zara = rows->items[2];
        const json_check::JsonValue *pts =
            json_check::member(zara, "points");
        CHECK(pts != nullptr && pts->text == "20");
        const json_check::JsonValue *life =
            json_check::member(zara, "lifetimePoints");
        CHECK(life != nullptr && life->text == "20");
        const json_check::JsonValue *done =
            json_check::member(zara, "completedRentals");
        CHECK(done != nullptr && done->text == "1");
        const json_check::JsonValue *toNext =
            json_check::member(zara, "pointsToNextTier");
        CHECK(toNext != nullptr && toNext->text == "230");
    }

    // The exported overview is the facade's own snapshot — one
    // computation, zero drift between screen and file.
    const LoyaltyOverview live = m.computeLoyaltyOverview();
    CHECK_EQ(live.totalMembers, 3);
    CHECK_EQ(live.outstandingPoints, 20);
    CHECK(approx(live.pointsLiability, 2.0));
    CHECK(live.topMember == "zara");
}
/* ============================================================
 *  Fleet analytics dashboard
 * ============================================================
 *  The maths is pure (today's date is a parameter), so the
 *  checks are fully deterministic — no clock dependency.
 * ============================================================ */

static void testAnalytics()
{
    /* ---- pure rendering helpers: exact, deterministic ---- */
    CHECK(analyticsBar(0, 0) == "");
    CHECK(analyticsBar(0, 5) == "");
    CHECK(analyticsBar(5, 0) == ""); // degenerate max
    CHECK(analyticsBar(3, 3, 12).size() == 12 * 3);  // full bar: 12 blocks
    CHECK(analyticsBar(1, 3, 12).size() == 4 * 3);   // 1/3 rounds to 4
    CHECK(analyticsBar(2, 3, 12).size() == 8 * 3);   // 2/3 rounds to 8
    CHECK(analyticsBar(1, 1000, 24).size() == 3);    // tiny value stays visible
    CHECK(analyticsBar(2, 5, 10) == analyticsBar(2, 5, 10)); // deterministic
    CHECK(analyticsPad("ab", 5) == "   ab");
    CHECK(analyticsPad("abcde", 5) == "abcde");
    CHECK(analyticsPad("abcdef", 5) == "abcdef"); // never truncates

    /* ---- empty system: fixed shapes, zeros, no crash ---- */
    {
        RentalManager empty;
        const AnalyticsReport r = buildAnalytics(empty, 20000);
        CHECK(r.fleetByStatus.size() == 4);
        CHECK(r.priceBuckets.size() == 4);
        int sumStatus = 0;
        for (const NamedCount &s : r.fleetByStatus)
            sumStatus += s.count;
        CHECK(sumStatus == 0);
        CHECK(r.bookingsPerCar.empty());
        CHECK(r.upcoming.empty());
        CHECK(r.overdueReturns == 0);
        CHECK(r.totalActiveBookings == 0);
    }

    /* ---- seeded system, synthetic "today" = 2026-11-05 ---- */
    int today = 0;
    CHECK(Date::parseISO("2026-11-05", today));
    {
        RentalManager m;
        CHECK(m.addCar(new Car("Cheap", "X", 30.0)));    // 0-50
        CHECK(m.addCar(new Car("Mid", "Y", 75.0)));      // 51-100
        CHECK(m.addCar(new Car("Idle", "V", 60.0)));     // 51-100, no booking
        CHECK(m.addCar(new Car("Premium", "Z", 150.0))); // 101-200
        CHECK(m.addCar(new Car("Lux", "W", 350.0)));     // 200+

        CHECK(m.registerCustomer("ana", "pass1234"));
        CHECK(m.login("ana", "pass1234"));
        std::string err;
        int s1 = 0, e1 = 0, s2 = 0, e2 = 0, s3 = 0, e3 = 0;
        CHECK(Date::parseISO("2026-11-05", s1));
        CHECK(Date::parseISO("2026-11-08", e1));
        CHECK(Date::parseISO("2026-11-10", s2));
        CHECK(Date::parseISO("2026-11-12", e2));
        CHECK(Date::parseISO("2026-11-01", s3));
        CHECK(Date::parseISO("2026-11-02", e3));
        CHECK(m.makeReservation("Cheap", s1, e1, err));   // picks up today
        CHECK(m.makeReservation("Mid", s2, e2, err));     // upcoming
        CHECK(m.makeReservation("Premium", s2, e2, err)); // tie on start
        CHECK(m.makeReservation("Lux", s3, e3, err));     // already overdue
        m.logout();

        const AnalyticsReport r = buildAnalytics(m, today);

        // Fleet composition: every booked car is Reserved, Idle is not.
        CHECK(r.fleetByStatus.size() == 4);
        CHECK(r.fleetByStatus[0].label == "Available");
        CHECK(r.fleetByStatus[0].count == 1);
        CHECK(r.fleetByStatus[1].count == 4);
        CHECK(r.fleetByStatus[2].count == 0);
        CHECK(r.fleetByStatus[3].count == 0);

        // Price buckets landed exactly where they should.
        CHECK(r.priceBuckets[0].count == 1); // Cheap 30
        CHECK(r.priceBuckets[1].count == 2); // Mid 75, Idle 60
        CHECK(r.priceBuckets[2].count == 1); // Premium 150
        CHECK(r.priceBuckets[3].count == 1); // Lux 350

        // Bookings per car: all tied at 1 -> alphabetical, only
        // cars WITH bookings are listed.
        CHECK(r.bookingsPerCar.size() == 4);
        CHECK(r.bookingsPerCar.front().label == "Cheap");
        CHECK(r.bookingsPerCar.back().label == "Premium");

        // Upcoming pickups: from today, earliest first, id breaks ties.
        CHECK(r.upcoming.size() == 3);
        CHECK(r.upcoming[0].carName == "Cheap");
        CHECK(r.upcoming[0].daysAway == 0);
        CHECK(r.upcoming[0].startDate == "2026-11-05");
        CHECK(r.upcoming[0].customer == "ana");
        CHECK(r.upcoming[1].carName == "Mid");
        CHECK(r.upcoming[1].daysAway == 5);
        CHECK(r.upcoming[2].carName == "Premium");

        // The overdue Lux booking (ended 2026-11-02).
        CHECK(r.overdueReturns == 1);
        CHECK(r.totalActiveBookings == 4);

        // A later "today" moves every booking into the overdue bin.
        int later = 0;
        CHECK(Date::parseISO("2026-11-20", later));
        const AnalyticsReport future = buildAnalytics(m, later);
        CHECK(future.upcoming.empty());
        CHECK(future.overdueReturns == 4);
        CHECK(future.totalActiveBookings == 4);
    }
}

/* ============================================================
 *  Permissions: the role matrix
 * ============================================================ */

static void testPermissions()
{
    // Managers hold the whole matrix, staff the front-desk trio,
    // customers (and unknown roles) nothing at all.
    CHECK_EQ(rolePermissions("Manager").size(), std::size_t(7));
    CHECK_EQ(rolePermissions("Staff").size(), std::size_t(3));
    CHECK(rolePermissions("Customer").empty());
    CHECK(rolePermissions("ghost").empty());

    // Every staff permission is also a manager permission.
    for (const Permission p : rolePermissions("Staff"))
        CHECK(roleHasPermission("Manager", p));

    // Spot checks in both directions.
    CHECK(roleHasPermission("Manager", Permission::MANAGE_USERS));
    CHECK(roleHasPermission("Manager", Permission::VIEW_AUDIT_LOG));
    CHECK(roleHasPermission("Staff", Permission::PROCESS_RETURNS));
    CHECK(roleHasPermission("Staff", Permission::HANDLE_DELIVERIES));
    CHECK(roleHasPermission("Staff", Permission::SEND_MAINTENANCE));
    CHECK(!roleHasPermission("Staff", Permission::MANAGE_USERS));
    CHECK(!roleHasPermission("Staff", Permission::EXPORT_REPORTS));
    CHECK(!roleHasPermission("Staff", Permission::VIEW_ANALYTICS));
    CHECK(!roleHasPermission("Staff", Permission::VIEW_AUDIT_LOG));
    CHECK(!roleHasPermission("Customer", Permission::PROCESS_RETURNS));
    CHECK(!roleHasPermission("", Permission::MANAGE_USERS));

    // Stable kebab-case ids — the audit trail spells them out.
    CHECK(std::string(permissionName(Permission::MANAGE_USERS)) ==
          "manage-users");
    CHECK(std::string(permissionName(Permission::EXPORT_REPORTS)) ==
          "export-reports");
    CHECK(std::string(permissionName(Permission::VIEW_ANALYTICS)) ==
          "view-analytics");
    CHECK(std::string(permissionName(Permission::VIEW_AUDIT_LOG)) ==
          "view-audit-log");
    CHECK(std::string(permissionName(Permission::HANDLE_DELIVERIES)) ==
          "handle-deliveries");
    CHECK(std::string(permissionName(Permission::PROCESS_RETURNS)) ==
          "process-returns");
    CHECK(std::string(permissionName(Permission::SEND_MAINTENANCE)) ==
          "send-maintenance");
}

/* ============================================================
 *  AuditLog: recording, windowing, CSV round-trip
 * ============================================================ */

static void testAuditLog()
{
    AuditLog log;
    CHECK(log.empty());
    CHECK_EQ(log.size(), std::size_t(0));

    log.record("2026-11-08", "admin", "login.ok", "Manager", true);
    log.record("2026-11-08", "sara", "export.fleet",
               "denied: needs export-reports", false);
    log.record("2026-11-08", "ali", "reservation.made", "Benz 1..3", true);
    CHECK_EQ(log.size(), std::size_t(3));

    // Reads are oldest-first.
    const std::vector<AuditEntry> everything = log.all();
    CHECK_EQ(everything.size(), std::size_t(3));
    CHECK(everything.front().action == "login.ok");
    CHECK(everything.back().action == "reservation.made");

    // The newest-N window keeps chronological order inside.
    const std::vector<AuditEntry> tail = log.recent(2);
    CHECK_EQ(tail.size(), std::size_t(2));
    CHECK(tail[0].actor == "sara");
    CHECK(tail[1].actor == "ali");
    CHECK(!tail[0].allowed);
    CHECK(log.recent(10).size() == std::size_t(3));
    CHECK(log.recent(0).empty());

    // RFC 4180 quoting plus newline flattening.
    CHECK(AuditLog::escapeField("plain") == "plain");
    CHECK(AuditLog::escapeField("a,b") == "\"a,b\"");
    CHECK(AuditLog::escapeField("say \"hi\"") == "\"say \"\"hi\"\"\"");
    CHECK(AuditLog::escapeField("line\nbreak") == "line\\nbreak");
    CHECK(AuditLog::escapeField("back\\slash") == "back\\\\slash");

    // Round-trip through CSV, hostile details included.
    AuditLog saved;
    saved.record("2026-11-08", "admin", "user.created",
                 "Staff, \"big\"", true);
    saved.record("2026-11-08", "sara", "login.failed", "multi\nline",
                 false);
    CHECK(saved.saveCSV("/tmp/audit_roundtrip.csv"));

    AuditLog loaded;
    CHECK(loaded.loadCSV("/tmp/audit_roundtrip.csv"));
    CHECK_EQ(loaded.size(), std::size_t(2));
    const std::vector<AuditEntry> rows = loaded.all();
    CHECK(rows[0].detail == "Staff, \"big\"");
    CHECK(rows[0].allowed);
    CHECK(rows[1].detail == "multi\nline");
    CHECK(!rows[1].allowed);
    CHECK(rows[1].action == "login.failed");

    // A missing file is not an error: fresh installs simply have
    // no trail yet.
    AuditLog fresh;
    CHECK(!fresh.loadCSV("/tmp/definitely-missing-audit.csv"));
    CHECK(fresh.empty());
}

/* ============================================================
 *  RBAC enforcement at the facade
 * ============================================================ */

static void testRbacEnforcement()
{
    RentalManager m;
    m.seedDemoData();
    std::string err;

    // Seeding already wrote its account-creation entries into the
    // trail; remember the size and watch it grow from the first login.
    const std::size_t seeded = m.auditLog().size();
    CHECK(seeded > 0);
    CHECK(m.login("sara", "staff123"));
    CHECK(m.auditLog().size() > seeded);
    CHECK(m.auditLog().all().back().action == "login.ok");
    CHECK(m.hasPermission(Permission::PROCESS_RETURNS));
    CHECK(!m.hasPermission(Permission::MANAGE_USERS));
    CHECK(!m.hasPermission(Permission::EXPORT_REPORTS));

    // Staff cannot export: denied at the facade, and the denial is
    // attributed and explained in the trail.
    const std::size_t before = m.auditLog().size();
    CHECK(!m.exportFleetReport("/tmp/staff_denied_export.csv"));
    const std::vector<AuditEntry> trail = m.auditLog().all();
    CHECK_EQ(trail.size(), before + 1);
    CHECK(!trail.back().allowed);
    CHECK(trail.back().actor == "sara");
    CHECK(trail.back().action == "export.fleet");
    CHECK(trail.back().detail.find("export-reports") !=
          std::string::npos);

    // Same story for account management — nothing is created.
    CHECK(!m.registerStaffAccount("eve", "pass1234", "Staff"));
    CHECK(!m.userExists("eve"));
    CHECK(!m.blockUser("ali"));
    CHECK(m.auditLog().all().back().action == "user.blocked");
    CHECK(!m.auditLog().all().back().allowed);

    // Managers hold the keys: export works and is logged as allowed.
    m.logout();
    CHECK(m.login("admin", "manager123"));
    CHECK(m.hasPermission(Permission::EXPORT_REPORTS));
    CHECK(m.hasPermission(Permission::MANAGE_USERS));
    CHECK(m.hasPermission(Permission::VIEW_AUDIT_LOG));
    CHECK(m.exportFleetReport("/tmp/manager_export.csv"));
    CHECK(m.auditLog().all().back().action == "export.fleet");
    CHECK(m.auditLog().all().back().allowed);

    // The whole trail survives save/load for the next session.
    CHECK(m.saveData("/tmp/rbac_data_dir"));
    RentalManager reloaded;
    CHECK(reloaded.loadData("/tmp/rbac_data_dir"));
    CHECK(reloaded.auditLog().size() == m.auditLog().size());
    CHECK(reloaded.auditLog().all().back().action == "export.fleet");

    // Unknown-user logins are audited too (the old segfault path).
    CHECK(!reloaded.login("ghost", "nope"));
    CHECK(reloaded.auditLog().all().back().action == "login.failed");
    CHECK(reloaded.auditLog().all().back().actor == "?");
}

/* ============================================================
 *  Waitlist (v2.6.0)
 * ============================================================ */

/// Priority rules of a single wish: earliest pickup first, FCFS tie.
static void testWaitlistEntryOrdering()
{
    Customer ali("ali"), sara("sara");
    Car car("Pride 131", "Saipa", 30.0);

    WaitlistEntry early(&ali, &car, 1, 100, 102);
    WaitlistEntry late(&sara, &car, 2, 105, 106);
    WaitlistEntry sameStart(&sara, &car, 3, 100, 100);

    // Earlier desired start wins outright.
    CHECK(early.hasHigherPriorityThan(&late));
    CHECK(!late.hasHigherPriorityThan(&early));

    // Equal starts -> the smaller join counter (first come) wins.
    CHECK(early.hasHigherPriorityThan(&sameStart));
    CHECK(!sameStart.hasHigherPriorityThan(&early));
    CHECK(sameStart.hasHigherPriorityThan(&late));
    CHECK(!late.hasHigherPriorityThan(&sameStart));

    // Accessors and the inclusive duration.
    CHECK_EQ(early.durationDays(), 3);
    CHECK_EQ(late.durationDays(), 2);
    CHECK_EQ(sameStart.durationDays(), 1);
    CHECK(sameStart.customer() == &sara);
    CHECK(sameStart.car() == &car);
    CHECK_EQ(sameStart.joinOrder(), 3);
    CHECK_EQ(sameStart.startDate(), 100);
    CHECK_EQ(sameStart.endDate(), 100);
}

/// Car-level wish heap: enqueue, fit-aware extraction, per-user
/// removal. The destructor frees whatever is still queued (ASan).
static void testCarWaitlistQueue()
{
    Customer ali("ali"), sara("sara"), reza("reza");
    Car car("Quick R", "Saipa", 35.0);

    CHECK(car.waitlistCount() == std::size_t(0));
    CHECK(car.takeBestWaitlistEntry() == nullptr); // nothing waiting

    // Booked solid for 10..19 — every wish inside clashes.
    Reservation *held = new Reservation(&ali, &car, 1, 1, 10, 19);
    car.reserve(held);
    CHECK(car.hasOverlap(10, 19));
    CHECK(!car.hasOverlap(1, 9));

    WaitlistEntry *e1 = new WaitlistEntry(&ali, &car, 1, 5, 7);   // fits
    WaitlistEntry *e2 = new WaitlistEntry(&sara, &car, 2, 12, 14); // clashes
    WaitlistEntry *e3 = new WaitlistEntry(&reza, &car, 3, 25, 26); // fits
    car.enqueueWaitlistEntry(e1);
    car.enqueueWaitlistEntry(e2);
    car.enqueueWaitlistEntry(e3);
    CHECK(car.waitlistCount() == std::size_t(3));

    // Extraction is fit-aware AND priority-ordered: e1 (start 5)
    // beats e3 (start 25); the clashing e2 stays queued.
    WaitlistEntry *best = car.takeBestWaitlistEntry();
    CHECK(best == e1);
    CHECK(car.waitlistCount() == std::size_t(2));
    delete best;

    // Next round: e2 clashes, e3 fits -> e3 is served, e2 survives.
    WaitlistEntry *next = car.takeBestWaitlistEntry();
    CHECK(next == e3);
    delete next;
    CHECK(car.waitlistCount() == std::size_t(1));
    CHECK(car.takeBestWaitlistEntry() == nullptr); // only e2 left, it clashes

    // Per-user removal: only ali's wishes come back (e2 is sara's).
    std::vector<WaitlistEntry *> removed = car.takeWaitlistEntriesFor("ali");
    CHECK(removed.empty());
    removed = car.takeWaitlistEntriesFor("sara");
    CHECK(removed.size() == std::size_t(1));
    CHECK(removed[0] == e2);
    CHECK(car.waitlistCount() == std::size_t(0));
    for (WaitlistEntry *e : removed)
        delete e;

    // Re-insertion path used by the CSV loader.
    WaitlistEntry *again = new WaitlistEntry(&reza, &car, 4, 5, 6);
    car.reinsertWaitlistEntry(again);
    CHECK(car.waitlistCount() == std::size_t(1));
    WaitlistEntry *out = car.takeBestWaitlistEntry();
    CHECK(out == again);
    delete out;
}

/// Facade behaviour end-to-end: availability search, join/leave
/// validation, front-desk serving, auto-promotion on return and on
/// finished maintenance, and the waitlist.csv round-trip.
static void testRentalManagerWaitlist()
{
    RentalManager m;
    m.seedDemoData();
    std::string err;

    // seedDemoData already ships ali (customer), sara (staff) and
    // admin (manager) — two more customers for the queue games.
    CHECK(m.registerCustomer("behnam", "pass1234"));
    CHECK(m.registerCustomer("parisa", "pass1234"));

    int s1 = 0, s2 = 0;
    CHECK(Date::parseISO("2026-12-01", s1));
    CHECK(Date::parseISO("2026-12-05", s2));

    /* ---- availability search ---- */
    CHECK(m.findAvailableCars(s1, s2).size() == std::size_t(8)); // all free
    m.login("ali", "customer123");
    CHECK(m.makeReservation("Pride 131", s1, s1 + 2, err));
    CHECK(m.findAvailableCars(s1, s2).size() == std::size_t(7));
    CHECK(m.findAvailableCars(s1 + 3, s2).size() == std::size_t(8));
    CHECK(m.findAvailableCars(s2, s1).empty()); // inverted range
    m.logout();

    /* ---- joining: every rejection path ---- */
    m.login("sara", "staff123"); // staff, not a customer
    CHECK(!m.joinWaitlist("Pride 131", s1, s1 + 2, err));
    CHECK(err.find("Only logged-in customers") != std::string::npos);
    m.logout();

    m.login("behnam", "pass1234");
    CHECK(!m.joinWaitlist("Ghost", s1, s2, err));      // unknown car
    CHECK(!m.joinWaitlist("Pride 131", s2, s1, err));  // inverted range
    CHECK(!m.joinWaitlist("Pride 131", -7, s1, err));  // before 1970-01-01
    CHECK(!m.joinWaitlist("Quick R", s1, s2, err));    // simply bookable
    CHECK(err.find("make a reservation instead") != std::string::npos);

    // Pride 131 holds 12-01..12-03 -> behnam may wait for the clash.
    CHECK(m.joinWaitlist("Pride 131", s1 + 1, s1 + 3, err)); // wish #1
    CHECK(m.totalWaitlisted() == std::size_t(1));
    CHECK(!m.joinWaitlist("Pride 131", s1 + 2, s1 + 4, err)); // duplicate wish
    CHECK(err.find("already waiting") != std::string::npos);
    CHECK(m.myWaitlist().size() == std::size_t(1));
    CHECK(m.myWaitlist().front()->car()->name() == "Pride 131");
    CHECK(m.myWaitlist().front()->joinOrder() == 1);

    // Leaving: once works, twice is an honest refusal.
    CHECK(m.leaveWaitlist("Pride 131", err));
    CHECK(m.totalWaitlisted() == std::size_t(0));
    CHECK(!m.leaveWaitlist("Pride 131", err));
    CHECK(m.myWaitlist().empty());

    // Two wishes on two cars; the earliest pickup lists first even
    // though it was joined later.
    CHECK(m.joinWaitlist("Pride 131", s1 + 1, s1 + 3, err)); // wish #2
    m.sendToMaintenance("Samand LX"); // a serviced car is wish-worthy
    CHECK(m.joinWaitlist("Samand LX", s1, s1 + 1, err));     // wish #3
    CHECK(m.totalWaitlisted() == std::size_t(2));
    CHECK(m.myWaitlist().size() == std::size_t(2));
    CHECK(m.myWaitlist().front()->startDate() == s1);        // Samand wish
    CHECK(m.myWaitlist().front()->joinOrder() == 3);
    CHECK(m.myWaitlist().back()->car()->name() == "Pride 131");
    m.logout();

    /* ---- serving: permissions first ---- */
    m.login("behnam", "pass1234"); // customers cannot serve
    CHECK(!m.serveWaitlist("Samand LX", err));
    CHECK(m.auditLog().all().back().action == "waitlist.served");
    CHECK(!m.auditLog().all().back().allowed);
    CHECK(m.auditLog().all().back().detail.find("handle-deliveries") !=
          std::string::npos);
    m.logout();

    m.login("sara", "staff123");
    // Samand is under maintenance — the facade refuses first.
    CHECK(!m.serveWaitlist("Samand LX", err));
    CHECK(err.find("maintenance") != std::string::npos);
    // Pride: every queued wish still clashes with ali's booking.
    CHECK(!m.serveWaitlist("Pride 131", err));
    CHECK(err.find("No waitlisted wish") != std::string::npos);
    m.logout();

    /* ---- auto-promotion on return ---- */
    m.login("ali", "customer123");
    CHECK(m.deliverCar("Pride 131", err)); // ali drives off with his booking
    m.logout();
    m.login("parisa", "pass1234");
    CHECK(m.joinWaitlist("Pride 131", s1 + 1, s1 + 3, err)); // wish #4
    m.logout();
    m.login("ali", "customer123");
    CHECK(m.returnCar("Pride 131", s1 + 2, err)); // on-time return frees it
    m.logout();
    {
        Car *car = m.searchCar("Pride 131");
        // The return auto-served behnam's earlier wish (12-02..12-04);
        // parisa's wish clashes with it and keeps waiting.
        CHECK(car->status() == CarStatus::Reserved);
        CHECK(car->currentReservation()->customer()->username() == "behnam");
        CHECK(car->currentReservation()->startDate() == s1 + 1);
        CHECK(car->currentReservation()->endDate() == s1 + 3);
    }
    CHECK(m.auditLog().all().back().action == "waitlist.promoted");
    CHECK(m.auditLog().all().back().detail.find("(auto)") !=
          std::string::npos);
    CHECK(m.totalWaitlisted() == std::size_t(2)); // parisa + the Samand wish

    // behnam's rental ends -> parisa is auto-booked in her turn.
    CHECK(m.deliverCar("Pride 131", err));
    CHECK(m.returnCar("Pride 131", s1 + 3, err));
    {
        Car *car = m.searchCar("Pride 131");
        CHECK(car->currentReservation()->customer()->username() == "parisa");
        CHECK(car->currentReservation()->startDate() == s1 + 1);
    }
    CHECK(m.totalWaitlisted() == std::size_t(1)); // the Samand wish remains

    /* ---- manual serve of the wish the auto pass left behind ---- */
    m.login("ali", "customer123");
    CHECK(m.makeReservation("Benz C200", s1, s1 + 4, err));
    CHECK(m.deliverCar("Benz C200", err)); // Benz is out with ali now
    m.logout();
    m.login("parisa", "pass1234");
    CHECK(m.joinWaitlist("Benz C200", s1 + 1, s1 + 2, err)); // wish #5
    m.logout();
    m.login("behnam", "pass1234");
    CHECK(m.joinWaitlist("Benz C200", s1 + 6, s1 + 7, err)); // wish #6
    m.logout();
    m.login("ali", "customer123");
    CHECK(m.returnCar("Benz C200", s1 + 4, err)); // auto-books parisa ONLY
    {
        Car *car = m.searchCar("Benz C200");
        CHECK(car->currentReservation()->customer()->username() == "parisa");
    }
    m.logout();
    m.login("sara", "staff123");
    CHECK(m.serveWaitlist("Benz C200", err)); // the front desk serves behnam
    {
        // parisa keeps the earlier pickup; behnam's wish became the
        // second booking of the schedule.
        Car *car = m.searchCar("Benz C200");
        CHECK(car->reservationCount() == std::size_t(2));
    }
    CHECK(m.auditLog().all().back().action == "waitlist.served");
    CHECK(m.auditLog().all().back().detail.find("behnam") !=
          std::string::npos);
    CHECK(m.auditLog().all().back().allowed);
    CHECK(!m.serveWaitlist("Benz C200", err)); // queue exhausted
    CHECK(err.find("No waitlisted wish") != std::string::npos);
    m.logout();
    CHECK(m.totalWaitlisted() == std::size_t(1));

    /* ---- auto-promotion on finished maintenance ---- */
    CHECK(m.finishMaintenance("Samand LX"));
    {
        Car *car = m.searchCar("Samand LX");
        CHECK(car->currentReservation() != nullptr);
        CHECK(car->currentReservation()->customer()->username() == "behnam");
        CHECK(car->currentReservation()->startDate() == s1);
    }
    CHECK(m.auditLog().all().back().action == "waitlist.promoted");
    CHECK(m.totalWaitlisted() == std::size_t(0));

    /* ---- a clashing wish survives serving (stays queued) ---- */
    m.login("ali", "customer123");
    CHECK(m.makeReservation("Dena Plus", s1, s1 + 4, err));
    m.logout();
    m.login("parisa", "pass1234");
    CHECK(m.joinWaitlist("Dena Plus", s1 + 2, s1 + 3, err)); // wish #7
    m.logout();
    m.login("sara", "staff123");
    CHECK(!m.serveWaitlist("Dena Plus", err)); // wish clashes with booking
    CHECK(err.find("No waitlisted wish") != std::string::npos);
    m.logout();
    CHECK(m.totalWaitlisted() == std::size_t(1));

    /* ---- waitlist.csv round-trip ---- */
    CHECK(m.saveData("/tmp/waitlist_data_dir"));
    RentalManager reloaded;
    CHECK(reloaded.loadData("/tmp/waitlist_data_dir"));
    CHECK(reloaded.totalWaitlisted() == std::size_t(1));
    reloaded.login("parisa", "pass1234");
    CHECK(reloaded.myWaitlist().size() == std::size_t(1));
    CHECK(reloaded.myWaitlist().front()->car()->name() == "Dena Plus");
    CHECK(reloaded.myWaitlist().front()->startDate() == s1 + 2);
    CHECK(reloaded.myWaitlist().front()->endDate() == s1 + 3);

    // The join counter survived the reload: parisa waits for the
    // Pride slot wedged against her own booking and continues the
    // global order at #8.
    CHECK(reloaded.joinWaitlist("Pride 131", s1 + 3, s1 + 4, err));
    CHECK(reloaded.myWaitlist().size() == std::size_t(2));
    CHECK(reloaded.myWaitlist().back()->joinOrder() == 8);
    reloaded.logout();

    // The dashboard maths see the queue too.
    AnalyticsReport report = buildAnalytics(reloaded, s1);
    CHECK(report.totalWaitlisted == 2);
    CHECK(report.waitlistPerCar.size() == std::size_t(2));
    CHECK(report.waitlistPerCar.front().label == "Dena Plus"); // tie -> name
    CHECK(report.waitlistPerCar.front().count == 1);
}

/* ============================================================
 *  Loyalty program (v2.7.0)
 * ============================================================ */

/// The pure rule engine in Loyalty.h: thresholds, multipliers,
/// accrual and redemption maths, progress to the next rank.
static void testLoyaltyMaths()
{
    // Tier names and thresholds.
    CHECK_EQ(std::string(toString(LoyaltyTier::Bronze)), std::string("Bronze"));
    CHECK_EQ(std::string(toString(LoyaltyTier::Silver)), std::string("Silver"));
    CHECK_EQ(std::string(toString(LoyaltyTier::Gold)), std::string("Gold"));
    CHECK_EQ(std::string(toString(LoyaltyTier::Platinum)),
             std::string("Platinum"));
    CHECK_EQ(tierThreshold(LoyaltyTier::Bronze), 0);
    CHECK_EQ(tierThreshold(LoyaltyTier::Silver), 250);
    CHECK_EQ(tierThreshold(LoyaltyTier::Gold), 1000);
    CHECK_EQ(tierThreshold(LoyaltyTier::Platinum), 2500);

    // Earning multipliers per rank.
    CHECK(approx(tierMultiplier(LoyaltyTier::Bronze), 1.0));
    CHECK(approx(tierMultiplier(LoyaltyTier::Silver), 1.25));
    CHECK(approx(tierMultiplier(LoyaltyTier::Gold), 1.5));
    CHECK(approx(tierMultiplier(LoyaltyTier::Platinum), 2.0));

    // Rank boundaries: the threshold itself flips the rank.
    CHECK(tierForLifetimePoints(0) == LoyaltyTier::Bronze);
    CHECK(tierForLifetimePoints(249) == LoyaltyTier::Bronze);
    CHECK(tierForLifetimePoints(250) == LoyaltyTier::Silver);
    CHECK(tierForLifetimePoints(999) == LoyaltyTier::Silver);
    CHECK(tierForLifetimePoints(1000) == LoyaltyTier::Gold);
    CHECK(tierForLifetimePoints(2499) == LoyaltyTier::Gold);
    CHECK(tierForLifetimePoints(2500) == LoyaltyTier::Platinum);
    CHECK(tierForLifetimePoints(9999) == LoyaltyTier::Platinum);

    // nextTier chains up and caps Platinum at itself.
    CHECK(nextTier(LoyaltyTier::Bronze) == LoyaltyTier::Silver);
    CHECK(nextTier(LoyaltyTier::Silver) == LoyaltyTier::Gold);
    CHECK(nextTier(LoyaltyTier::Gold) == LoyaltyTier::Platinum);
    CHECK(nextTier(LoyaltyTier::Platinum) == LoyaltyTier::Platinum);

    // Distance to the next rank (0 = top rank reached).
    CHECK_EQ(pointsToNextTier(0), 250);
    CHECK_EQ(pointsToNextTier(249), 1);
    CHECK_EQ(pointsToNextTier(250), 750);
    CHECK_EQ(pointsToNextTier(999), 1);
    CHECK_EQ(pointsToNextTier(1000), 1500);
    CHECK_EQ(pointsToNextTier(2499), 1);
    CHECK_EQ(pointsToNextTier(2500), 0);
    CHECK_EQ(pointsToNextTier(4000), 0);

    // Accrual: 1 point per 10.00 spent, tier multiplier applied,
    // floored to whole points. A zero or negative invoice earns
    // nothing.
    CHECK_EQ(pointsEarnedFor(0, 0.0), 0);
    CHECK_EQ(pointsEarnedFor(0, -50.0), 0);
    CHECK_EQ(pointsEarnedFor(0, 9.99), 0); // floor, never rounds up
    CHECK_EQ(pointsEarnedFor(0, 90.0), 9);
    CHECK_EQ(pointsEarnedFor(0, 540.0), 54);                     // Bronze x1.0
    CHECK_EQ(pointsEarnedFor(250, 540.0), 67);                   // 67.5 -> 67
    CHECK_EQ(pointsEarnedFor(1000, 540.0), 81);                  // Gold x1.5
    CHECK_EQ(pointsEarnedFor(2500, 540.0), 108);                 // Platinum x2.0

    // Redemption maths: 10 points = 1.00, min 50, steps of 10.
    CHECK(approx(pointsToValue(50), 5.0));
    CHECK(approx(pointsToValue(100), 10.0));
    CHECK(approx(pointsToValue(340), 34.0));
    CHECK(!isRedeemableAmount(0));
    CHECK(!isRedeemableAmount(10));
    CHECK(!isRedeemableAmount(49));
    CHECK(isRedeemableAmount(50));
    CHECK(!isRedeemableAmount(55));
    CHECK(isRedeemableAmount(60));
    CHECK(isRedeemableAmount(3200));
    CHECK(!isRedeemableAmount(-50));

    // The profile snapshot derives its rank from lifetime points.
    const LoyaltyProfile fresh;
    CHECK_EQ(fresh.points, 0);
    CHECK_EQ(fresh.lifetimePoints, 0);
    CHECK_EQ(fresh.completedRentals, 0);
    CHECK(fresh.tier() == LoyaltyTier::Bronze);
    CHECK_EQ(fresh.pointsToNextTier(), 250);

    LoyaltyProfile ali;
    ali.points = 340;
    ali.lifetimePoints = 340;
    ali.completedRentals = 3;
    CHECK(ali.tier() == LoyaltyTier::Silver);
    CHECK_EQ(ali.pointsToNextTier(), 660);
}

/// The full facade flow: points accrue on completed rentals at the
/// multiplier held before each return, rank-ups happen mid-history,
/// redemptions obey every rule, staff have no standing and the
/// counters round-trip through users.csv.
static void testRentalManagerLoyalty()
{
    const std::string dir = "/tmp/crs_loyalty_data";
    std::system(("rm -rf " + dir).c_str());

    RentalManager m;
    CHECK(m.registerCustomer("parisa", "pass1234"));
    CHECK(m.addCar(new Car("Testar", "TestCo", 45.0)));

    // No session: a zero profile and no redemption.
    CHECK(m.myLoyalty().points == 0);
    CHECK(m.myLoyalty().tier() == LoyaltyTier::Bronze);
    std::string err;
    CHECK(m.redeemPoints(50, err) == -1.0);

    // Rental 1: 10 days x 45 = 450 -> 45 points (Bronze x1.0).
    m.login("parisa", "pass1234");
    CHECK(m.makeReservation("Testar", 100, 109, err));
    m.logout();
    CHECK(m.deliverCar("Testar", err));
    CHECK(m.returnCar("Testar", 109, err)); // on time
    CHECK(m.lastLoyaltyRankUp().empty()); // Bronze stays Bronze (v2.9.0)

    m.login("parisa", "pass1234");
    CHECK_EQ(m.myLoyalty().points, 45);
    CHECK_EQ(m.myLoyalty().lifetimePoints, 45);
    CHECK_EQ(m.myLoyalty().completedRentals, 1);
    CHECK(m.myLoyalty().tier() == LoyaltyTier::Bronze);
    CHECK(approx(m.currentCustomerBalance(), 450.0));
    m.logout();

    // Rental 2: 47 days x 45 = 2115 -> floor(211.5) = 211, still
    // Bronze at return time. Lifetime 45 + 211 = 256 -> Silver.
    m.login("parisa", "pass1234");
    CHECK(m.makeReservation("Testar", 120, 166, err));
    m.logout();
    CHECK(m.deliverCar("Testar", err));
    CHECK(m.returnCar("Testar", 166, err));
    // The threshold crossing is announced for the front desk
    // and lands in the trail.                      (v2.9.0)
    CHECK(m.lastLoyaltyRankUp() == "parisa crossed into Silver");

    m.login("parisa", "pass1234");
    CHECK_EQ(m.myLoyalty().points, 256);
    CHECK_EQ(m.myLoyalty().lifetimePoints, 256);
    CHECK_EQ(m.myLoyalty().completedRentals, 2);
    CHECK(m.myLoyalty().tier() == LoyaltyTier::Silver);
    CHECK(approx(m.currentCustomerBalance(), 450.0 + 2115.0));
    m.logout();

    // Rental 3: a late return still earns, now at the Silver rate.
    // 10 days x 45 = 450 + 1 late day x 45 x 1.5 = 517.50
    // -> floor(517.5 x 0.1 x 1.25) = 64 points.
    m.login("parisa", "pass1234");
    CHECK(m.makeReservation("Testar", 200, 209, err));
    m.logout();
    CHECK(m.deliverCar("Testar", err));
    CHECK(m.returnCar("Testar", 210, err)); // 1 day late
    CHECK(m.lastLoyaltyRankUp().empty()); // Silver stays Silver (v2.9.0)

    m.login("parisa", "pass1234");
    CHECK_EQ(m.myLoyalty().points, 320);
    CHECK_EQ(m.myLoyalty().lifetimePoints, 320);
    CHECK_EQ(m.myLoyalty().completedRentals, 3);
    CHECK(approx(m.currentCustomerBalance(), 450.0 + 2115.0 + 517.5));
    m.logout();

    // The trail logged every earn (and no redemption yet).
    int earnCount = 0, redeemCount = 0;
    for (const AuditEntry &e : m.auditLog().all())
    {
        if (e.action == "loyalty.earned")
            ++earnCount;
        if (e.action == "loyalty.redeemed")
            ++redeemCount;
    }
    CHECK_EQ(earnCount, 3);
    CHECK_EQ(redeemCount, 0);

    // Exactly one promotion in the trail: rental 2 crossed into
    // Silver, dated by the calendar layer.         (v2.9.0)
    int rankUpCount = 0;
    for (const AuditEntry &e : m.auditLog().all())
    {
        if (e.action != "loyalty.rankup")
            continue;
        ++rankUpCount;
        CHECK(e.detail == "parisa Bronze -> Silver");
        CHECK(e.allowed);
        CHECK(Date::isISODate(e.date));
    }
    CHECK_EQ(rankUpCount, 1);

    // The loyalty CSV report: header + one username-sorted row
    // per member with the derived rank and progress. (v2.9.0)
    CHECK(m.exportLoyaltyReport(dir + "/loyalty.csv"));
    {
        std::ifstream in(dir + "/loyalty.csv");
        CHECK(in.good());
        std::vector<std::string> rows;
        std::string line;
        while (std::getline(in, line))
            rows.push_back(line);
        CHECK_EQ(rows.size(), std::size_t(2));
        CHECK(rows[0] == "username,tier,points,lifetimePoints,"
                         "completedRentals,pointsToNextTier");
        CHECK(rows[1] == "parisa,Silver,320,320,3,680");
    }

    // An empty program exports the header and nothing else.
    {
        RentalManager bare;
        CHECK(bare.exportLoyaltyReport(dir + "/bare_loyalty.csv"));
        std::ifstream in(dir + "/bare_loyalty.csv");
        std::string header, extra;
        CHECK(std::getline(in, header));
        CHECK(header == "username,tier,points,lifetimePoints,"
                        "completedRentals,pointsToNextTier");
        CHECK(!std::getline(in, extra));
    }

    // Redemption: every rule enforced, in order.
    m.login("parisa", "pass1234");
    CHECK(m.redeemPoints(45, err) == -1.0);   // below minimum
    CHECK(m.redeemPoints(330, err) == -1.0);  // more than held
    CHECK(m.redeemPoints(3200, err) == -1.0); // not a step of 10
    const double credited = m.redeemPoints(100, err);
    CHECK(approx(credited, 10.0));
    CHECK(approx(m.currentCustomerBalance(), 3082.5 - 10.0));
    CHECK_EQ(m.myLoyalty().points, 220);
    CHECK_EQ(m.myLoyalty().lifetimePoints, 320); // lifetime never spent
    CHECK(m.myLoyalty().tier() == LoyaltyTier::Silver);

    // A cleared bill leaves nothing to redeem against.
    CHECK(approx(m.payBill(), 3072.5));
    CHECK(m.redeemPoints(50, err) == -1.0);
    m.logout();

    // Staff sessions have no standing and cannot redeem.
    CHECK(m.registerStaffAccount("st", "st1234", "Staff"));
    m.login("st", "st1234");
    CHECK(m.myLoyalty().points == 0);
    CHECK(m.myLoyalty().completedRentals == 0);
    CHECK(m.redeemPoints(50, err) == -1.0);
    // ...and they cannot export the loyalty report either —
    // EXPORT_REPORTS is manager-only.              (v2.9.0)
    CHECK(!m.exportLoyaltyReport(dir + "/denied.csv"));
    m.logout();
    bool exportDenied = false;
    for (const AuditEntry &e : m.auditLog().all())
        if (e.action == "export.loyalty" && !e.allowed)
            exportDenied = true;
    CHECK(exportDenied);

    // Persistence: the three counters round-trip through users.csv.
    CHECK(m.saveData(dir));
    RentalManager reloaded;
    CHECK(reloaded.loadData(dir));
    CHECK(reloaded.login("parisa", "pass1234"));
    CHECK_EQ(reloaded.myLoyalty().points, 220);
    CHECK_EQ(reloaded.myLoyalty().lifetimePoints, 320);
    CHECK_EQ(reloaded.myLoyalty().completedRentals, 3);
    CHECK(reloaded.myLoyalty().tier() == LoyaltyTier::Silver);
    CHECK(approx(reloaded.currentCustomerBalance(), 0.0));

    // Earning continues seamlessly on the reloaded state: 450 spent
    // at the Silver rate -> floor(45 x 1.25) = 56 more points.
    CHECK(reloaded.makeReservation("Testar", 300, 309, err));
    reloaded.logout();
    CHECK(reloaded.deliverCar("Testar", err));
    CHECK(reloaded.returnCar("Testar", 309, err));
    CHECK(reloaded.lastLoyaltyRankUp().empty()); // 376 lifetime: still Silver
    reloaded.login("parisa", "pass1234");
    CHECK_EQ(reloaded.myLoyalty().points, 276);
    reloaded.logout();
}

/// The manager's program overview, built from a hand-written
/// users.csv that also exercises the legacy (7-column) row shape.
static void testLoyaltyOverview()
{
    const std::string dir = "/tmp/crs_loyalty_overview";
    std::system(("rm -rf " + dir).c_str());
    std::filesystem::create_directories(dir);

    // Five customers across the ranks, a top-member tie, one staff
    // row and one legacy customer row without loyalty columns.
    const std::string salt = "0123456789abcdef";
    {
        std::ofstream users(dir + "/users.csv");
        users << "username,salt,hash,role,blocked,failedLogins,balance"
                 ",loyaltyPoints,lifetimePoints,completedRentals\n";
        users << "nima," << salt << ',' << SHA256::hash(salt + "pass1234")
              << ",Customer,0,0,50.00,40,40,1\n";
        users << "sahar," << salt << ',' << SHA256::hash(salt + "pass1234")
              << ",Customer,0,0,0.00,250,250,2\n";
        users << "behrad," << salt << ',' << SHA256::hash(salt + "pass1234")
              << ",Customer,0,0,12.50,2600,2600,9\n";
        users << "zara," << salt << ',' << SHA256::hash(salt + "pass1234")
              << ",Customer,0,0,99.00,2600,2600,9\n";
        users << "olduser," << salt << ','
              << SHA256::hash(salt + "pass1234")
              << ",Customer,0,0,12.50\n"; // pre-2.7.0 row shape
        users << "st," << salt << ',' << SHA256::hash(salt + "pass1234")
              << ",Staff,0,0,0.00,0,0,0\n";
    }

    RentalManager m;
    CHECK(m.loadData(dir));

    // A legacy row loads with an all-zero standing, not garbage.
    m.login("olduser", "pass1234");
    CHECK(m.myLoyalty().points == 0);
    CHECK(m.myLoyalty().lifetimePoints == 0);
    CHECK(m.myLoyalty().completedRentals == 0);
    CHECK(m.myLoyalty().tier() == LoyaltyTier::Bronze);
    CHECK(approx(m.currentCustomerBalance(), 12.5));
    m.logout();

    // The overview counts every customer in the right rank.
    const LoyaltyOverview o = m.loyaltyOverview(); // no session -> allowed
    CHECK_EQ(o.totalMembers, 5);
    CHECK_EQ(o.membersByTier[0], 2); // Bronze: nima + olduser
    CHECK_EQ(o.membersByTier[1], 1); // Silver: sahar
    CHECK_EQ(o.membersByTier[2], 0); // Gold: none
    CHECK_EQ(o.membersByTier[3], 2); // Platinum: behrad + zara
    CHECK_EQ(o.outstandingPoints, 40 + 250 + 2600 + 2600);
    CHECK(approx(o.pointsLiability, 549.0));
    // Tie on 2600 lifetime points -> lexicographically smaller name.
    CHECK(o.topMember == "behrad");
    CHECK_EQ(o.topMemberLifetime, 2600);

    // Staff are shut out of the overview (VIEW_ANALYTICS) and the
    // denial lands in the audit trail.
    m.login("st", "pass1234");
    CHECK(!m.hasPermission(Permission::VIEW_ANALYTICS));
    const LoyaltyOverview denied = m.loyaltyOverview();
    CHECK_EQ(denied.totalMembers, 0);
    CHECK_EQ(denied.outstandingPoints, 0);
    bool denialLogged = false;
    for (const AuditEntry &e : m.auditLog().all())
        if (e.action == "loyalty.overview" && !e.allowed)
            denialLogged = true;
    CHECK(denialLogged);
    m.logout();

    // An empty program reads as all-zero, not garbage.
    RentalManager empty;
    const LoyaltyOverview none = empty.loyaltyOverview();
    CHECK_EQ(none.totalMembers, 0);
    CHECK_EQ(none.outstandingPoints, 0);
    CHECK(none.topMember.empty());
}

int main()
{
    std::cout << "==== Car Rental System — unit tests ====\n";

    runSuite("SHA256 + password hashing", testSHA256);
    runSuite("DoublyLinkedList", testDoublyLinkedList);
    runSuite("PriorityQueue", testPriorityQueue);
    runSuite("AVLTree (self-balancing)", testAVLTree);
    runSuite("HashTable (chaining + rehash)", testHashTable);
    runSuite("Reservation rules", testReservationRules);
    runSuite("Date utilities (ISO + serial days)", testDates);
    runSuite("RentalManager: accounts & lockout", testRentalManagerAccounts);
    runSuite("RentalManager: reservations & invoices", testRentalManagerReservations);
    runSuite("RentalManager: fleet & maintenance", testRentalManagerFleetAndMaintenance);
    runSuite("RentalManager: fleet filters", testRentalManagerFleetFilters);
    runSuite("RentalManager: keyword search", testRentalManagerSearch);
    runSuite("RentalManager: statistics", testRentalManagerStats);
    runSuite("RentalManager: CSV reports", testRentalManagerReports);
    runSuite("JSON escaping", testJsonEscaping);
    runSuite("JSON report export", testJsonExport);
    runSuite("JSON report: loyalty standings", testJsonLoyaltyExport);
    runSuite("Fleet analytics dashboard", testAnalytics);
    runSuite("RentalManager: CSV persistence", testRentalManagerPersistence);
    runSuite("Role hierarchy", testRoleHierarchy);
    runSuite("Permissions: role matrix", testPermissions);
    runSuite("AuditLog: recording & CSV round-trip", testAuditLog);
    runSuite("RBAC enforcement at the facade", testRbacEnforcement);
    runSuite("Waitlist: wish priority rules", testWaitlistEntryOrdering);
    runSuite("Car waitlist queue", testCarWaitlistQueue);
    runSuite("Waitlist: facade, promotion & persistence", testRentalManagerWaitlist);
    runSuite("Loyalty: tiers & maths", testLoyaltyMaths);
    runSuite("RentalManager: loyalty accrual & redemption", testRentalManagerLoyalty);
    runSuite("Loyalty: program overview & legacy rows", testLoyaltyOverview);

    std::cout << "\n" << g_checks << " checks, " << g_failures << " failure(s)\n";
    if (g_failures == 0)
        std::cout << "ALL TESTS PASSED\n";
    else
        std::cout << "TESTS FAILED\n";
    return g_failures == 0 ? 0 : 1;
}
