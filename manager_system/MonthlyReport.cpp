#include "MonthlyReport.h"

#include <algorithm>
#include <cstdio>

/* ============================================================
 *  buildMonthlyReport — one pass over the ledger   (v3.0.0)
 * ============================================================ */

MonthlyReport buildMonthlyReport(const RentalHistory &history, int year,
                                 int month)
{
    MonthlyReport report;
    if (month < 1 || month > 12 || year < 1 || year > 9999)
        return report; // invalid month reads as all-zero

    char prefix[16];
    std::snprintf(prefix, sizeof prefix, "%04d-%02d-", year, month);
    report.month = std::string(prefix, prefix + 7); // "YYYY-MM"

    std::vector<CarRevenue> cars;
    std::vector<std::string> customers;
    int dayCounts[32] = {0};

    history.forEach([&](const RentalRecord &r)
                    {
                        if (r.returnISO.rfind(prefix, 0) != 0)
                            return; // completed in another month

                        ++report.rentalsCompleted;
                        report.revenue += r.total;
                        report.discountGiven += r.discount;
                        report.lateFees += r.penalty;

                        if (std::find(customers.begin(), customers.end(),
                                      r.customer) == customers.end())
                            customers.push_back(r.customer);

                        bool known = false;
                        for (CarRevenue &car : cars)
                        {
                            if (car.name == r.carName)
                            {
                                ++car.rentals;
                                car.revenue += r.total;
                                known = true;
                                break;
                            }
                        }
                        if (!known)
                            cars.push_back(CarRevenue{r.carName, 1, r.total});

                        // Day of month from the ISO string — the
                        // return date is already calendar-validated.
                        int day = 0;
                        if (r.returnISO.size() == 10)
                            day = std::stoi(r.returnISO.substr(8, 2));
                        if (day >= 1 && day <= 31)
                            ++dayCounts[day];
                    });

    report.distinctCustomers = static_cast<int>(customers.size());

    // Revenue order (desc), name breaks ties — the deterministic
    // ordering rule every other report in this project follows.
    std::sort(cars.begin(), cars.end(),
              [](const CarRevenue &a, const CarRevenue &b)
              {
                  if (a.revenue != b.revenue)
                      return a.revenue > b.revenue;
                  return a.name < b.name;
              });
    report.perCar = std::move(cars);

    for (int day = 1; day <= 31; ++day)
    {
        if (dayCounts[day] == 0)
            continue;
        if (dayCounts[day] > report.topDayRentals)
        {
            report.topDay = day;
            report.topDayRentals = dayCounts[day];
        }
        char label[4];
        std::snprintf(label, sizeof label, "%02d", day);
        report.perDay.push_back(NamedCount{label, dayCounts[day]});
    }
    return report;
}
