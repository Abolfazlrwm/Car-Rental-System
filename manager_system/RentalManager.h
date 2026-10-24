#ifndef RENTAL_MANAGER_H
#define RENTAL_MANAGER_H

#include <string>
#include <vector>

#include "../basic_structures/AVLTree.h"
#include "../basic_structures/DoublyLinkedList.h"
#include "../basic_structures/HashTable.h"
#include "Car.h"
#include "User.h"

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
 *  Persistence: the whole state round-trips through three CSV
 *  files in ./data (users.csv, cars.csv, reservations.csv).
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
    std::size_t fleetSize() const { return carOrder_.size(); }

    /// Cars in insertion order (manager's perspective).
    void forEachCar(const std::function<void(Car *)> &fn) const;

    /// Cars sorted by name via the AVL in-order traversal.
    void forEachCarSorted(const std::function<void(Car *)> &fn) const;

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

    static const double PENALTY_RATE; // late-return multiplier (1.5x/day)

    // ---- internal helpers ----
    Customer *asCustomer() const; // logged-in user as Customer (or null)
    void clearAll();

    // Classic polynomial rolling hash (base 31) used for username indexing.
    static std::size_t stringHash(const std::string &key);
};

#endif // RENTAL_MANAGER_H
