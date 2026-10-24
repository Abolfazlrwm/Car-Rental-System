#include "Reservation.h"

Reservation::Reservation(Customer *customer, Car *car, int id,
                         int requestTime, int startDate, int endDate)
    : customer_(customer), car_(car), id_(id),
      requestTime_(requestTime), startDate_(startDate), endDate_(endDate) {}

bool Reservation::hasHigherPriorityThan(const Reservation *other) const
{
    // Primary criterion: the sooner the pickup, the higher the priority.
    if (startDate_ != other->startDate_)
        return startDate_ < other->startDate_;

    // Tie-break: whoever asked first is served first (FIFO fairness).
    return requestTime_ < other->requestTime_;
}
