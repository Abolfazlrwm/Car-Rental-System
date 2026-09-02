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
             { return a->hasHigherPriorityThan(b); })
{
}

Car::~Car()
{
    // Free every booking still waiting in the schedule.
    queue_.forEach([](Reservation *r)
                   { delete r; });
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
    delete r;

    // Drain and re-insert to restore the heap invariant.
    while (!queue_.empty())
        (void)queue_.extractMin();
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
