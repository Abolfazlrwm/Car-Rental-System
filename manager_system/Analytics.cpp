#include "Analytics.h"

#include <algorithm>
#include <vector>

#include "Car.h"
#include "Date.h"
#include "RentalManager.h"
#include "Reservation.h"
#include "User.h"

std::string analyticsBar(int count, int maxCount, int width)
{
    if (count <= 0 || maxCount <= 0 || width <= 0)
        return "";
    // Rounded proportional fill, clamped to the field and never
    // empty while the value it represents is non-zero.
    const double ratio = static_cast<double>(count) / maxCount;
    int blocks = static_cast<int>(ratio * width + 0.5);
    blocks = std::max(1, std::min(width, blocks));

    // Raw UTF-8 bytes of U+2588 (FULL BLOCK) — byte escapes keep
    // every compiler/platform on the UTF-8 console convention.
    static const char kBlock[] = "\xE2\x96\x88";
    std::string out;
    out.reserve(static_cast<std::size_t>(blocks) * 3);
    for (int i = 0; i < blocks; ++i)
        out += kBlock;
    return out;
}

std::string analyticsPad(const std::string &text, std::size_t width)
{
    if (text.size() >= width)
        return text;
    return std::string(width - text.size(), ' ') + text;
}

AnalyticsReport buildAnalytics(const RentalManager &mgr, int todaySerial)
{
    AnalyticsReport report;

    // Fixed four-slice layout (CarStatus enum order), so the
    // dashboard keeps its shape even when a status is empty.
    report.fleetByStatus = {{"Available", 0},
                            {"Reserved", 0},
                            {"Rented", 0},
                            {"Maintenance", 0}};

    // Fixed daily-rate buckets.
    report.priceBuckets = {{"0-50", 0}, {"51-100", 0}, {"101-200", 0},
                           {"200+", 0}};

    mgr.forEachCar([&](Car *car)
                   {
                       ++report.fleetByStatus[static_cast<int>(car->status())]
                              .count;

                       const double price = car->pricePerDay();
                       const int bucket = price <= 50.0   ? 0
                                          : price <= 100.0 ? 1
                                          : price <= 200.0 ? 2
                                                           : 3;
                       ++report.priceBuckets[bucket].count;

                       car->forEachReservation(
                           [&](const Reservation *r)
                           {
                               ++report.totalActiveBookings;
                               if (r->endDate() < todaySerial)
                                   ++report.overdueReturns;
                           });
                   });

    // Bookings per car: only cars that actually have bookings,
    // most-booked first, name breaks ties (deterministic output).
    std::vector<NamedCount> perCar;
    mgr.forEachCar([&](Car *car)
                   {
                       if (car->reservationCount() > 0)
                           perCar.push_back({car->name(),
                                             static_cast<int>(
                                                 car->reservationCount())});
                   });
    std::sort(perCar.begin(), perCar.end(),
              [](const NamedCount &a, const NamedCount &b)
              {
                  if (a.count != b.count)
                      return a.count > b.count;
                  return a.label < b.label;
              });
    report.bookingsPerCar = std::move(perCar);

    // Upcoming pickups from today onwards, earliest first.
    std::vector<const Reservation *> scheduled;
    mgr.forEachCar([&](Car *car)
                   {
                       car->forEachReservation(
                           [&](const Reservation *r)
                           {
                               if (r->startDate() >= todaySerial)
                                   scheduled.push_back(r);
                           });
                   });
    std::sort(scheduled.begin(), scheduled.end(),
              [](const Reservation *a, const Reservation *b)
              {
                  if (a->startDate() != b->startDate())
                      return a->startDate() < b->startDate();
                  return a->id() < b->id();
              });
    const std::size_t kMaxUpcoming = 5;
    for (std::size_t i = 0; i < scheduled.size() && i < kMaxUpcoming; ++i)
    {
        const Reservation *r = scheduled[i];
        report.upcoming.push_back({r->car()->name(),
                                   r->customer()->username(),
                                   Date::toISO(r->startDate()),
                                   r->startDate() - todaySerial});
    }

    return report;
}
