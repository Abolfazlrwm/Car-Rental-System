#ifndef RESERVATION_H
#define RESERVATION_H

#include <string>

class Customer;
class Car;

/**
 * ============================================================
 *  Reservation
 * ============================================================
 *  One rental booking, linking a Customer to a Car for the
 *  inclusive date range [startDate .. endDate].
 *
 *  Dates are represented as integer day numbers (1, 2, 3, ...)
 *  to keep the focus on the scheduling logic; a std::tm based
 *  calendar is listed in the README roadmap.
 *
 *  Priority rule (used by the per-car PriorityQueue):
 *    1. Earlier startDate wins  (soonest pickup is served first)
 *    2. Tie -> earlier requestTime wins (first-come-first-served)
 * ============================================================
 */
class Reservation
{
public:
    Reservation(Customer *customer, Car *car, int id,
                int requestTime, int startDate, int endDate);

    int id() const { return id_; }
    int requestTime() const { return requestTime_; }
    int startDate() const { return startDate_; }
    int endDate() const { return endDate_; }
    Customer *customer() const { return customer_; }
    Car *car() const { return car_; }

    /// Used by "renew rental": push the return date further out.
    void extendEndDate(int extraDays) { endDate_ += extraDays; }

    /// True when `this` must be served before `other` (heap comparator).
    bool hasHigherPriorityThan(const Reservation *other) const;

    /// Inclusive length of the booking in days.
    int durationDays() const { return endDate_ - startDate_ + 1; }

    /// Date-interval overlap test: two ranges clash when both
    /// newStart <= resEnd AND newEnd >= resStart.
    static bool rangesOverlap(int aStart, int aEnd, int bStart, int bEnd)
    {
        return aStart <= bEnd && aEnd >= bStart;
    }

private:
    Customer *customer_; // not owned (owned by the user table)
    Car *car_;           // not owned (owned by the car index)
    int id_;
    int requestTime_;
    int startDate_;
    int endDate_;
};

#endif // RESERVATION_H
