#ifndef WAITLIST_ENTRY_H
#define WAITLIST_ENTRY_H

class Customer;
class Car;

/**
 * ============================================================
 *  WaitlistEntry   (v2.6.0)
 * ============================================================
 *  One customer waiting for a car slot that does not exist yet:
 *  every direct booking for their desired range was rejected
 *  (an overlapping booking holds the car, it is rented out or
 *  under maintenance), so they file themselves into the car's
 *  waitlist and get auto-booked the moment a matching window
 *  frees up.
 *
 *  Priority rule (used by the per-car PriorityQueue):
 *    1. Earlier desired startDate wins (soonest wish served first)
 *    2. Tie -> smaller joinOrder wins (first-come-first-served)
 *
 *  ...an exact mirror of the Reservation priority rule, so the
 *  two heaps of a car behave consistently.
 * ============================================================
 */
class WaitlistEntry
{
public:
    WaitlistEntry(Customer *customer, Car *car, int joinOrder,
                  int startDate, int endDate);

    Customer *customer() const { return customer_; }
    Car *car() const { return car_; }
    int joinOrder() const { return joinOrder_; }
    int startDate() const { return startDate_; }
    int endDate() const { return endDate_; }

    /// Inclusive length of the desired rental in days.
    int durationDays() const { return endDate_ - startDate_ + 1; }

    /// True when `this` must be served before `other` (heap comparator).
    bool hasHigherPriorityThan(const WaitlistEntry *other) const;

private:
    Customer *customer_; // not owned (owned by the user table)
    Car *car_;           // not owned (owned by the car index)
    int joinOrder_;      // global arrival counter, FCFS tie-break
    int startDate_;      // desired range start (serial day)
    int endDate_;        // desired range end (serial day, inclusive)
};

#endif // WAITLIST_ENTRY_H
