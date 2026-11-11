#include "Car.h"

#include "User.h" // full Customer definition for reservationsOf()

std::string toString(CarStatus status)
{
    switch (status)
    {
    case CarStatus::Available:
        return "AVAILABLE";
    case CarStatus::Reserved:
        return "RESERVED";
    case CarStatus::Rented:
        return "RENTED";
    case CarStatus::Maintenance:
        return "MAINTENANCE";
    }
    return "UNKNOWN";
}

Car::Car(const std::string &name, const std::string &brand, double pricePerDay)
    : name_(name),
      brand_(brand),
      pricePerDay_(pricePerDay),
      status_(CarStatus::Available),
      totalIncome_(0.0),
      maintenanceCost_(0.0),
      // Heap order: Reservation::hasHigherPriorityThan decides who is next.
      queue_([](const Reservation *a, const Reservation *b)
             { return a->hasHigherPriorityThan(b); }),
      // Same priority shape for the wish queue (v2.6.0).
      waitlist_([](const WaitlistEntry *a, const WaitlistEntry *b)
                { return a->hasHigherPriorityThan(b); })
{
}

Car::~Car()
{
    // Free every booking still waiting in the schedule and every
    // wish still waiting for a slot.
    queue_.forEach([](Reservation *r)
                   { delete r; });
    waitlist_.forEach([](WaitlistEntry *e)
                      { delete e; });
}

void Car::reserve(Reservation *r)
{
    queue_.insert(r);
    if (status_ == CarStatus::Available)
        status_ = CarStatus::Reserved;
}

bool Car::hasOverlap(int start, int end) const
{
    bool clash = false;
    queue_.forEach([&](const Reservation *r)
                   {
                       if (Reservation::rangesOverlap(start, end, r->startDate(), r->endDate()))
                           clash = true;
                   });
    return clash;
}

void Car::dropReservation(Reservation *r)
{
    // Rebuild the heap without the given booking (simple + safe: the
    // schedule is tiny, so an O(n) rebuild is perfectly fine here).
    std::vector<Reservation *> keep;
    queue_.forEach([&](Reservation *cur)
                   {
                       if (cur != r)
                           keep.push_back(cur);
                   });

    // Drain FIRST, free LAST: extractMin() re-heapifies and would
    // still touch `r` — deleting before draining was a use-after-free
    // (found by the v3.0.0 series-cancellation flow under ASan).
    while (!queue_.empty())
        (void)queue_.extractMin();
    delete r;

    for (Reservation *cur : keep)
        queue_.insert(cur);
}

std::vector<Reservation *> Car::reservationsOf(const std::string &username) const
{
    std::vector<Reservation *> result;
    queue_.forEach([&](Reservation *r)
                   {
                       if (r->customer()->username() == username)
                           result.push_back(r);
                   });
    return result;
}
void Car::reinsertReservation(Reservation *r)
{
    queue_.insert(r);
}

void Car::enqueueWaitlistEntry(WaitlistEntry *e)
{
    waitlist_.insert(e);
}

WaitlistEntry *Car::takeBestWaitlistEntry()
{
    // Drain in priority order: the first wish whose range does not
    // clash with the booked schedule wins; everything else goes
    // back into the heap. Same O(n) rebuild trick as
    // dropReservation() — the queue is tiny at front-desk scale.
    std::vector<WaitlistEntry *> rest;
    WaitlistEntry *best = nullptr;
    while (!waitlist_.empty())
    {
        WaitlistEntry *e = waitlist_.extractMin();
        if (!best && !hasOverlap(e->startDate(), e->endDate()))
            best = e;
        else
            rest.push_back(e);
    }
    for (WaitlistEntry *e : rest)
        waitlist_.insert(e);
    return best;
}

std::vector<WaitlistEntry *> Car::takeWaitlistEntriesFor(const std::string &username)
{
    std::vector<WaitlistEntry *> removed, keep;
    while (!waitlist_.empty())
    {
        WaitlistEntry *e = waitlist_.extractMin();
        if (e->customer()->username() == username)
            removed.push_back(e);
        else
            keep.push_back(e);
    }
    for (WaitlistEntry *e : keep)
        waitlist_.insert(e);
    return removed;
}

void Car::refreshStatus()
{
    // Maintenance is managed explicitly by finishMaintenance(); everything
    // else is fully determined by whether bookings are still queued.
    if (status_ == CarStatus::Maintenance)
        return;

    status_ = queue_.empty() ? CarStatus::Available : CarStatus::Reserved;
}

void Car::addMaintenanceLog(const std::string &date,
                            const std::string &description, double cost)
{
    maintenanceLog_.emplace_back(date, description, cost);
    maintenanceCost_ += cost;
}
