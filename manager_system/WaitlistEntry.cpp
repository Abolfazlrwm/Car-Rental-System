#include "WaitlistEntry.h"

#include "Car.h"
#include "User.h"

WaitlistEntry::WaitlistEntry(Customer *customer, Car *car, int joinOrder,
                             int startDate, int endDate)
    : customer_(customer),
      car_(car),
      joinOrder_(joinOrder),
      startDate_(startDate),
      endDate_(endDate)
{
}

bool WaitlistEntry::hasHigherPriorityThan(const WaitlistEntry *other) const
{
    // Same shape as Reservation::hasHigherPriorityThan: the wish
    // with the earliest desired pickup is served first; a tie is
    // broken by the global join counter (first come, first served).
    if (startDate_ != other->startDate_)
        return startDate_ < other->startDate_;
    return joinOrder_ < other->joinOrder_;
}
