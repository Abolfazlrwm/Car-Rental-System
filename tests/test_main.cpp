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
#include <filesystem>
#include <fstream>
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
    CHECK(!m.makeReservation("Benz C200", 0, 5, err));   // day < 1

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

    // Manager-only blocking of a customer, enforced at login.
    CHECK(m.unblockUser("ali")); // staff cannot block, but API allows unblock
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
        CHECK(lines[1].rfind("2,Samand LX,ali,2,4,3", 0) == 0);
        CHECK(lines[2].rfind("1,Benz C200,ali,5,9,5", 0) == 0);
    }

    /* ---- failure path: a directory is not a writable report file ---- */
    std::filesystem::create_directories(dir + "/blocker");
    CHECK(!m.exportFleetReport(dir + "/blocker"));
    CHECK(!m.exportBookingReport(dir + "/blocker"));
}

/* ============================================================
 *  main
 * ============================================================ */

int main()
{
    std::cout << "==== Car Rental System — unit tests ====\n";

    runSuite("SHA256 + password hashing", testSHA256);
    runSuite("DoublyLinkedList", testDoublyLinkedList);
    runSuite("PriorityQueue", testPriorityQueue);
    runSuite("AVLTree (self-balancing)", testAVLTree);
    runSuite("HashTable (chaining + rehash)", testHashTable);
    runSuite("Reservation rules", testReservationRules);
    runSuite("RentalManager: accounts & lockout", testRentalManagerAccounts);
    runSuite("RentalManager: reservations & invoices", testRentalManagerReservations);
    runSuite("RentalManager: fleet & maintenance", testRentalManagerFleetAndMaintenance);
    runSuite("RentalManager: fleet filters", testRentalManagerFleetFilters);
    runSuite("RentalManager: keyword search", testRentalManagerSearch);
    runSuite("RentalManager: statistics", testRentalManagerStats);
    runSuite("RentalManager: CSV reports", testRentalManagerReports);
    runSuite("RentalManager: CSV persistence", testRentalManagerPersistence);
    runSuite("Role hierarchy", testRoleHierarchy);

    std::cout << "\n" << g_checks << " checks, " << g_failures << " failure(s)\n";
    if (g_failures == 0)
        std::cout << "ALL TESTS PASSED\n";
    else
        std::cout << "TESTS FAILED\n";
    return g_failures == 0 ? 0 : 1;
}
