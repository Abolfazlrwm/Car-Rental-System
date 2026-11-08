#ifndef RENTAL_MANAGER_H
#define RENTAL_MANAGER_H

#include <string>
#include <vector>

#include "../basic_structures/AVLTree.h"
#include "../basic_structures/DoublyLinkedList.h"
#include "../basic_structures/HashTable.h"
#include "AuditLog.h"
#include "Car.h"
#include "Loyalty.h"
#include "Permissions.h"
#include "User.h"
#include "WaitlistEntry.h"

/**
 * ============================================================
 *  FleetStats
 * ============================================================
 *  Aggregate snapshot of the whole system, computed on demand by
 *  RentalManager::computeStats(). Printed by the manager's
 *  "Fleet statistics" screen and reused by the CSV report exporter,
 *  so the on-screen numbers and the exported file can never drift
 *  apart.
 * ============================================================
 */
struct FleetStats
{
    std::size_t totalCars = 0;
    std::size_t availableCars = 0;
    std::size_t reservedCars = 0;
    std::size_t rentedCars = 0;
    std::size_t maintenanceCars = 0;
    std::size_t activeBookings = 0; // queued reservations fleet-wide

    double grossIncome = 0.0;      // sum of the per-car income ledgers
    double maintenanceSpend = 0.0; // sum of maintenance costs
    double netRevenue = 0.0;       // grossIncome - maintenanceSpend

    /// Percentage of the fleet currently earning or promised
    /// (rented + reserved), 0..100; 0 when the fleet is empty.
    double utilization = 0.0;

    /// Name of the highest-earning car, "" when the fleet is empty
    /// or nothing has earned yet.
    std::string topEarner;
};

/**
 * ============================================================
 *  RentalManager
 * ============================================================
 *  Facade that owns every entity of the system and exposes the
 *  business operations to the CLI.
 *
 *  Container layout (who owns what):
 *
 *    HashTable<string, User*>  userIndex_   OWNS all users
 *    AVLTree<string, Car*>     carIndex_    OWNS all cars
 *    DoublyLinkedList<string>  carOrder_    insertion-order names
 *        (kept so "view fleet" can show cars in the exact order
 *         the manager added them; the AVL index additionally
 *         offers a name-sorted view)
 *
 *    Car      owns its Reservation* queue
 *    Reservation points to Customer/Car but owns neither
 *
 *  This guarantees every heap allocation is released exactly
 *  once — the original implementation leaked cars, users,
 *  reservations and maintenance records, and dereferenced a
 *  null user pointer on login of unknown usernames (segfault).
 *
 *  Persistence: the whole state round-trips through CSV files in
 *  ./data (users.csv, cars.csv, reservations.csv, audit_log.csv,
 *  waitlist.csv). Customer rows in users.csv also carry the three
 *  loyalty counters since v2.7.0; pre-2.7.0 files still load.
 * ============================================================
 */
class RentalManager
{
public:
    RentalManager();
    ~RentalManager();

    // The manager owns everything; copying is meaningless.
    RentalManager(const RentalManager &) = delete;
    RentalManager &operator=(const RentalManager &) = delete;

    /* ---------------- Account management ---------------- */

    /// Self-registration is limited to Customer accounts. Returns false
    /// when the username is taken or the password is too short (< 4).
    bool registerCustomer(const std::string &username,
                          const std::string &password);

    /// Manager-only: create Staff or extra Manager accounts.
    bool registerStaffAccount(const std::string &username,
                              const std::string &password,
                              const std::string &role /* Staff | Manager */);

    /// Null-safe login (the original crashed here on unknown usernames).
    /// Blocks the account automatically after 3 failed attempts.
    bool login(const std::string &username, const std::string &password);

    void logout();
    bool isLoggedIn() const;
    std::string currentUserRole() const;
    std::string currentUsername() const;

    bool blockUser(const std::string &username);   // returns false if absent
    bool unblockUser(const std::string &username); // returns false if absent
    bool userExists(const std::string &username) const;
    bool isUserBlocked(const std::string &username) const;

    /* ---------------- Fleet management ---------------- */

    /// Takes ownership of the car. Returns false on duplicate name.
    bool addCar(Car *car);

    Car *searchCar(const std::string &name);
    std::vector<Car *> findCarsByBrand(const std::string &brand) const;

    /// Cars that can be booked right now (status Available or
    /// Reserved), in the manager's insertion order.
    std::vector<Car *> findBookableCars() const;

    /// Cars renting for minPrice..maxPrice per day (both inclusive),
    /// sorted by daily rate ascending — the customer's "what fits my
    /// budget" query.
    std::vector<Car *> findCarsInPriceRange(double minPrice,
                                            double maxPrice) const;

    std::size_t fleetSize() const { return carOrder_.size(); }

    /// Cars in insertion order (manager's perspective).
    void forEachCar(const std::function<void(Car *)> &fn) const;

    /// Cars sorted by name via the AVL in-order traversal.
    void forEachCarSorted(const std::function<void(Car *)> &fn) const;

    /// Whole fleet ordered by daily rate ascending (name breaks ties),
    /// without disturbing the insertion-order view.
    void forEachCarByPrice(const std::function<void(Car *)> &fn) const;

    /// Case-insensitive keyword search across car names AND brands
    /// ("peugeot" finds every Peugeot, "206" finds the Peugeot 206).
    /// Matches come back in the manager's insertion order; an empty
    /// keyword matches nothing, so a blank prompt never dumps the fleet.
    std::vector<Car *> searchFleet(const std::string &keyword) const;

    /// Cars that can be booked for [start..end] RIGHT NOW: bookable
    /// status AND no queued booking overlaps the range, in the
    /// manager's insertion order. Invalid ranges match nothing —
    /// the caller is told why the fleet looks empty.  (v2.6.0)
    std::vector<Car *> findAvailableCars(int start, int end) const;

    bool sendToMaintenance(const std::string &carName);
    bool finishMaintenance(const std::string &carName);

    /* ---------------- Customer operations ---------------- */

    /// Double-booking-safe reservation for the logged-in customer.
    /// Returns false when the car is unknown, under maintenance, the
    /// range is invalid, or the dates overlap another booking.
    bool makeReservation(const std::string &carName, int startDate,
                         int endDate, std::string &errorOut);

    /// All bookings of the logged-in customer across the whole fleet.
    std::vector<Reservation *> myReservations() const;

    /// Extend the customer's active rental. Fails when the car is not
    /// currently rented by them or later bookings would be pushed.
    bool renewRental(const std::string &carName, int extraDays,
                     std::string &errorOut);

    /// Pay off the logged-in customer's balance. Returns the paid amount
    /// (-1 when the user is not a customer).
    double payBill();

    double currentCustomerBalance() const;

    /* ---------------- Waitlist (v2.6.0) ---------------- */

    /// File the logged-in customer into the car's waitlist for
    /// [startDate..endDate]. Refused when the car is unknown, the
    /// range is invalid, the car is directly bookable for the range
    /// ("make a reservation instead") or the customer already waits
    /// for an overlapping range on the same car.
    bool joinWaitlist(const std::string &carName, int startDate,
                      int endDate, std::string &errorOut);

    /// Withdraw every wish the logged-in customer filed for this
    /// car. False when there is nothing to withdraw.
    bool leaveWaitlist(const std::string &carName, std::string &errorOut);

    /// The logged-in customer's wishes across the whole fleet,
    /// earliest desired pickup first.
    std::vector<WaitlistEntry *> myWaitlist() const;

    /// Front-desk action (HANDLE_DELIVERIES permission): book the
    /// best-fitting waitlisted wish of a car right now. Fails when
    /// no queued wish fits the schedule yet.
    bool serveWaitlist(const std::string &carName, std::string &errorOut);

    /// Customers currently waiting fleet-wide (stats + dashboard).
    std::size_t totalWaitlisted() const;

    /* ---------------- Loyalty program (v2.7.0) ----------------
     *  Rules live in Loyalty.h; the facade owns the three moments
     *  that touch state: points are EARNED when a return closes a
     *  rental, SPENT when a customer redeems them against their
     *  bill, and COUNTED here for the manager's program overview.
     */

    /// The logged-in customer's standing (zero profile for
    /// staff / manager sessions and before login).
    LoyaltyProfile myLoyalty() const;

    /// Turn points into a bill credit: 10 points = 1.00, minimum
    /// 50, whole currency only, never more than the outstanding
    /// bill (points are spent, not cashed out). Returns the credited
    /// amount, or -1.0 with the reason in `errorOut`.
    double redeemPoints(int points, std::string &errorOut);

    /// Program snapshot for the manager screen: members per tier,
    /// outstanding points and their credit liability, top member.
    /// Gated by VIEW_ANALYTICS, like the rest of the dashboards.
    LoyaltyOverview loyaltyOverview() const;

    /* ---------------- Staff operations ---------------- */

    /// Hand the car over to the customer holding the earliest booking.
    bool deliverCar(const std::string &carName, std::string &errorOut);

    /// Process a return: computes duration charge + late penalty in one
    /// place (the original added income TWICE — once on booking, once on
    /// return — inflating every report).
    bool returnCar(const std::string &carName, int actualReturnDay,
                   std::string &errorOut);

    bool recordMaintenance(const std::string &carName, const std::string &date,
                           const std::string &description, double cost,
                           std::string &errorOut);

    /// Per-car revenue + maintenance costs + grand totals.
    void printIncomeReport() const;

    /* ---------------- Reporting ---------------- */

    /// One-pass snapshot of fleet composition, bookings and money.
    /// Never throws, never depends on login state.
    FleetStats computeStats() const;

    /// Write the whole fleet to `path` as CSV (header + one row per
    /// car, sorted by name). Missing parent directories are created;
    /// returns false when the file cannot be opened for writing.
    bool exportFleetReport(const std::string &path) const;

    /// Write every live reservation to `path` as CSV, ordered by start
    /// date (reservation id breaks ties — the per-car heaps are not
    /// ordered views). Missing parent directories are created.
    bool exportBookingReport(const std::string &path) const;

    /* ---------------- Audit trail (v2.5.0) ---------------- */

    /// Append-only trail of privileged actions, permission denials
    /// and login events (oldest first). The facade records the
    /// entries itself — feature code cannot forget to log — and
    /// the trail round-trips through data/audit_log.csv.
    const AuditLog &auditLog() const { return auditLog_; }

    /* ---------------- Permissions (v2.5.0) ---------------- */

    /// Does the LOGGED-IN user hold `p`? False when nobody is
    /// logged in — the matrix governs interactive sessions.
    bool hasPermission(Permission p) const;

    /* ---------------- Persistence ---------------- */

    /// Write users.csv / cars.csv / reservations.csv under `dataDir`.
    bool saveData(const std::string &dataDir = "data") const;

    /// Load a previously saved state (users -> cars -> reservations).
    /// Returns false when the directory does not exist yet.
    bool loadData(const std::string &dataDir = "data");

    /// One-time demo fleet + accounts so a fresh clone is instantly usable.
    void seedDemoData();

    bool hasDataOnDisk(const std::string &dataDir = "data") const;

private:
    // ---- containers & ownership ----
    HashTable<std::string, User *> userIndex_;
    AVLTree<std::string, Car *> carIndex_;
    DoublyLinkedList<std::string> carOrder_;

    User *currentUser_;
    int reservationCounter_;
    int waitlistCounter_; // global join order, survives save/load

    // Audit writes also happen from const methods (exports, save),
    // so the trail is a mutable side-ledger.
    mutable AuditLog auditLog_;

    static const double PENALTY_RATE; // late-return multiplier (1.5x/day)

    // ---- internal helpers ----
    Customer *asCustomer() const; // logged-in user as Customer (or null)
    void clearAll();

    /// Record one audit entry stamped with today's ISO date and the
    /// logged-in username ("?" when no session is active).
    void audit(const std::string &action, const std::string &detail,
               bool allowed = true) const;

    /// Permission gate for manager-exclusive calls: passes through
    /// when no session is active (programmatic use: scripts, tests,
    /// seeding), otherwise checks the role matrix and records a
    /// denial in the audit trail. Returns true when allowed.
    bool permitted(Permission p, const std::string &action) const;

    /// Turn a waitlisted wish into a real booking (new reservation
    /// id, heap insert). Returns the booking so the caller can
    /// audit it; the wish itself is consumed and freed.
    Reservation *bookWaitlistEntry(Car *car, WaitlistEntry *entry);

    /// Auto-book the best-fitting wish after a car freed up (a
    /// return was processed, maintenance finished). Silent no-op
    /// when nothing fits or the car is under maintenance.
    void tryPromoteWaitlist(Car *car);

    // Classic polynomial rolling hash (base 31) used for username indexing.
    static std::size_t stringHash(const std::string &key);
};

#endif // RENTAL_MANAGER_H
