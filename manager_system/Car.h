#ifndef CAR_H
#define CAR_H

#include <string>
#include <vector>

#include "../basic_structures/PriorityQueue.h"
#include "Reservation.h"

/**
 * ============================================================
 *  Car
 * ============================================================
 *  A rentable vehicle plus everything attached to it:
 *    - a PriorityQueue<Reservation*> booking schedule
 *    - an income ledger (rent revenue)
 *    - a maintenance history log
 *
 *  Lifecycle of the reservation queue:
 *    reserve()    -> enqueue booking, status becomes RESERVED
 *    deliver()    -> front booking is being served, status RENTED
 *    complete()   -> dequeue front booking (return processed)
 *
 *  Ownership: the Car OWNS its Reservation objects (they are
 *  deleted when dequeued or when the car is destroyed), but it
 *  does NOT own the Customer objects they point to.
 * ============================================================
 */
enum class CarStatus
{
    Available,
    Reserved,
    Rented,
    Maintenance
};

/// Human-readable status names (used in the CLI and in CSV files).
std::string toString(CarStatus status);

/// One completed or scheduled maintenance intervention.
struct MaintenanceRecord
{
    std::string date;
    std::string description;
    double cost;

    MaintenanceRecord(const std::string &d, const std::string &desc, double c)
        : date(d), description(desc), cost(c) {}
};

class Car
{
public:
    Car(const std::string &name, const std::string &brand, double pricePerDay);
    ~Car();

    // Cars are owned by the AVL index; copying is disabled.
    Car(const Car &) = delete;
    Car &operator=(const Car &) = delete;

    /* ---------------- Basic getters ---------------- */
    const std::string &name() const { return name_; }
    const std::string &brand() const { return brand_; }
    double pricePerDay() const { return pricePerDay_; }
    CarStatus status() const { return status_; }
    void setStatus(CarStatus s) { status_ = s; }
    bool isAvailableForBooking() const
    {
        return status_ == CarStatus::Available || status_ == CarStatus::Reserved;
    }

    /* ---------------- Money ---------------- */
    double totalIncome() const { return totalIncome_; }
    double maintenanceCost() const { return maintenanceCost_; }
    double netRevenue() const { return totalIncome_ - maintenanceCost_; }
    void addIncome(double amount) { totalIncome_ += amount; }
    void addMaintenanceCost(double amount) { maintenanceCost_ += amount; }

    /* ---------------- Reservation schedule ---------------- */

    /// Enqueue a booking (assumes overlap has already been rejected).
    void reserve(Reservation *r);

    /// True when any queued booking clashes with [start..end].
    bool hasOverlap(int start, int end) const;

    bool hasReservations() const { return !queue_.empty(); }
    std::size_t reservationCount() const { return queue_.size(); }

    /// The booking that will be served next (earliest pickup). nullptr if none.
    Reservation *currentReservation() const
    {
        return queue_.empty() ? nullptr : queue_.peek();
    }

    /// Dequeue and RETURN the served booking — the caller (RentalManager)
    /// reads its data for the invoice, then deletes it.
    Reservation *completeReservation() { return queue_.extractMin(); }

    /// Destroy a booking that belongs to this car (data cleanup / imports).
    void dropReservation(Reservation *r);

    /// Bookings whose customer matches `username` (for "my reservations").
    std::vector<Reservation *> reservationsOf(const std::string &username) const;

    /// Re-insert a booking (used when restoring state from CSV).
    void reinsertReservation(Reservation *r);

    /// Read-only visit of every queued booking (used by CSV export).
    void forEachReservation(const std::function<void(const Reservation *)> &fn) const
    {
        queue_.forEach([&](const Reservation *r)
                       { fn(r); });
    }

    /// Refresh status based on the queue: empty -> Available, else Reserved.
    void refreshStatus();

    /* ---------------- Maintenance ---------------- */
    void addMaintenanceLog(const std::string &date,
                           const std::string &description, double cost);
    const std::vector<MaintenanceRecord> &maintenanceLog() const
    {
        return maintenanceLog_;
    }

private:
    std::string name_;
    std::string brand_;
    double pricePerDay_;
    CarStatus status_;
    double totalIncome_;
    double maintenanceCost_;

    PriorityQueue<Reservation *> queue_;
    std::vector<MaintenanceRecord> maintenanceLog_;
};

#endif // CAR_H
