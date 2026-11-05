#include "JsonExport.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "Car.h"
#include "Date.h"
#include "RentalManager.h"
#include "Reservation.h"
#include "User.h"

std::string jsonEscape(const std::string &text)
{
    std::string out;
    out.reserve(text.size() + 8);
    for (const unsigned char c : text)
    {
        switch (c)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (c < 0x20)
            {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04X", c);
                out += buf;
            }
            else
            {
                out += static_cast<char>(c);
            }
        }
    }
    return out;
}

namespace
{

/* JSON string literal: quoted + escaped. */
std::string jstr(const std::string &text)
{
    return '"' + jsonEscape(text) + '"';
}

/* Numbers go through the same default double formatting the CSV
 * reports use, so both files always agree on the figures. */
std::string jnum(double value)
{
    std::ostringstream out;
    out << value;
    return out.str();
}

} // namespace

bool exportJsonReport(const RentalManager &manager, const std::string &path)
{
    const std::filesystem::path parent =
        std::filesystem::path(path).parent_path();
    if (!parent.empty())
        std::filesystem::create_directories(parent);

    std::ofstream file(path);
    if (!file)
        return false;

    const FleetStats stats = manager.computeStats();

    file << "{\n";
    file << "  \"report\": \"car-rental-system\",\n";
    file << "  \"generatedAt\": " << jstr(Date::toISO(Date::todaySerial()))
         << ",\n";

    /* ---------------- stats snapshot ---------------- */
    file << "  \"stats\": {\n";
    file << "    \"totalCars\": " << stats.totalCars << ",\n";
    file << "    \"availableCars\": " << stats.availableCars << ",\n";
    file << "    \"reservedCars\": " << stats.reservedCars << ",\n";
    file << "    \"rentedCars\": " << stats.rentedCars << ",\n";
    file << "    \"maintenanceCars\": " << stats.maintenanceCars << ",\n";
    file << "    \"activeBookings\": " << stats.activeBookings << ",\n";
    file << "    \"grossIncome\": " << jnum(stats.grossIncome) << ",\n";
    file << "    \"maintenanceSpend\": " << jnum(stats.maintenanceSpend)
         << ",\n";
    file << "    \"netRevenue\": " << jnum(stats.netRevenue) << ",\n";
    file << "    \"utilization\": " << jnum(stats.utilization) << ",\n";
    file << "    \"topEarner\": " << jstr(stats.topEarner) << "\n";
    file << "  },\n";

    /* ---------------- fleet (name-sorted, like the CSV sheet) ---- */
    file << "  \"fleet\": [\n";
    bool firstCar = true;
    manager.forEachCarSorted([&](Car *car)
                             {
                                 if (!firstCar)
                                     file << ",\n";
                                 firstCar = false;
                                 file << "    {\n";
                                 file << "      \"name\": "
                                      << jstr(car->name()) << ",\n";
                                 file << "      \"brand\": "
                                      << jstr(car->brand()) << ",\n";
                                 file << "      \"pricePerDay\": "
                                      << jnum(car->pricePerDay()) << ",\n";
                                 file << "      \"status\": "
                                      << jstr(toString(car->status()))
                                      << ",\n";
                                 file << "      \"income\": "
                                      << jnum(car->totalIncome()) << ",\n";
                                 file << "      \"maintenanceCost\": "
                                      << jnum(car->maintenanceCost())
                                      << ",\n";
                                 file << "      \"netRevenue\": "
                                      << jnum(car->netRevenue()) << ",\n";
                                 file << "      \"maintenanceLog\": [";
                                 bool firstRecord = true;
                                 for (const MaintenanceRecord &record :
                                      car->maintenanceLog())
                                 {
                                     if (!firstRecord)
                                         file << ", ";
                                     firstRecord = false;
                                     file << "{\"date\": "
                                          << jstr(record.date)
                                          << ", \"description\": "
                                          << jstr(record.description)
                                          << ", \"cost\": "
                                          << jnum(record.cost) << "}";
                                 }
                                 file << "]\n";
                                 file << "    }";
                             });
    file << "\n  ],\n";

    /* ---------------- bookings (ordered by pickup, like the CSV) -- */
    std::vector<const Reservation *> bookings;
    manager.forEachCarSorted([&](Car *car)
                             {
                                 car->forEachReservation(
                                     [&](const Reservation *r)
                                     { bookings.push_back(r); });
                             });
    std::sort(bookings.begin(), bookings.end(),
              [](const Reservation *a, const Reservation *b)
              {
                  if (a->startDate() != b->startDate())
                      return a->startDate() < b->startDate();
                  return a->id() < b->id();
              });

    file << "  \"bookings\": [\n";
    bool firstBooking = true;
    for (const Reservation *r : bookings)
    {
        if (!firstBooking)
            file << ",\n";
        firstBooking = false;
        file << "    {\n";
        file << "      \"id\": " << r->id() << ",\n";
        file << "      \"carName\": " << jstr(r->car()->name()) << ",\n";
        file << "      \"customer\": " << jstr(r->customer()->username())
             << ",\n";
        file << "      \"startDate\": " << jstr(Date::toISO(r->startDate()))
             << ",\n";
        file << "      \"endDate\": " << jstr(Date::toISO(r->endDate()))
             << ",\n";
        file << "      \"durationDays\": " << r->durationDays() << "\n";
        file << "    }";
    }
    file << "\n  ]\n";
    file << "}\n";
    return true;
}
