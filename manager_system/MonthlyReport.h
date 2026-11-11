#ifndef MONTHLY_REPORT_H
#define MONTHLY_REPORT_H

#include <string>
#include <vector>

#include "Analytics.h" // NamedCount, the bar-chart slices
#include "RentalHistory.h"

/**
 * ============================================================
 *  MonthlyReport — the manager's month at a glance   (v3.0.0)
 * ============================================================
 *  Pure aggregation over the completed-rentals ledger, no I/O and
 *  no mutable state: buildMonthlyReport() filters the ledger by
 *  the ACTUAL return month (revenue is booked when a return is
 *  processed, not when a booking is made) and distills the numbers
 *  the console screen renders. Same layering as Analytics (v2.4.0)
 *  — the maths lives here so every figure is unit-testable and
 *  mainApp stays a pure renderer.
 * ============================================================
 */

/// One car's share of a month: how many of its rentals were
/// completed and how much they invoiced.
struct CarRevenue
{
    std::string name;
    int rentals = 0;
    double revenue = 0.0;
};

struct MonthlyReport
{
    /// "YYYY-MM" the report was built for, empty when the month
    /// is invalid (the report reads as all-zero).
    std::string month;

    int rentalsCompleted = 0;
    double revenue = 0.0;      // invoiced totals (net of discounts,
                               // including late fees)
    double discountGiven = 0.0; // sum of the discounts the pricing
                               // engine took off the base charges
    double lateFees = 0.0;     // sum of the penalties
    int distinctCustomers = 0; // customers with >= 1 completed rental

    /// Cars ordered by revenue (desc, name breaks ties). Cars with
    /// no completed rental this month are absent.
    std::vector<CarRevenue> perCar;

    /// Days of the month that saw at least one return, ascending,
    /// labelled "01".."31" — the bar chart of the screen.
    std::vector<NamedCount> perDay;

    /// Busiest day (1..31, earliest breaks ties; 0 when empty) and
    /// how many returns it saw.
    int topDay = 0;
    int topDayRentals = 0;
};

/// Aggregate every ledger record whose ACTUAL return falls in
/// `year`-`month` (month 1..12). An invalid month yields an empty
/// report with an empty `month` label.
MonthlyReport buildMonthlyReport(const RentalHistory &history, int year,
                                 int month);

#endif // MONTHLY_REPORT_H
