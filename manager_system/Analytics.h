#ifndef ANALYTICS_H
#define ANALYTICS_H

#include <cstddef>
#include <string>
#include <vector>

class RentalManager;

/**
 * ============================================================
 *  Analytics — data layer of the fleet analytics dashboard
 * ============================================================
 *  Pure computation, no I/O: buildAnalytics() reads the live
 *  state through RentalManager's public queries and distills
 *  it into the numbers the console dashboard renders. Keeping
 *  the math here instead of the UI makes every figure
 *  unit-testable and keeps mainApp a pure renderer.
 *
 *  All "days" are serial days from the Date layer, so the
 *  dashboard inherits the 2.3.0 calendar correctness (leap
 *  years included) — "in 3 days" really means 3 days.
 * ============================================================
 */

/// One labelled bar of the dashboard (fleet composition, price
/// buckets, bookings per car).
struct NamedCount
{
    std::string label;
    int count = 0;
};

/// A live booking that has not been picked up yet.
struct UpcomingPickup
{
    std::string carName;
    std::string customer;
    std::string startDate; // ISO "YYYY-MM-DD"
    int daysAway = 0;      // pickup - today (>= 0)
};

struct AnalyticsReport
{
    /// Always four slices in fixed enum order:
    /// Available, Reserved, Rented, Maintenance.
    std::vector<NamedCount> fleetByStatus;

    /// Cars with at least one live booking, most-booked first,
    /// name breaks ties.
    std::vector<NamedCount> bookingsPerCar;

    /// Cars with at least one waitlisted customer (v2.6.0),
    /// deepest queue first, name breaks ties.
    std::vector<NamedCount> waitlistPerCar;

    /// Fleet split over four fixed daily-rate buckets:
    /// "0-50", "51-100", "101-200", "200+".
    std::vector<NamedCount> priceBuckets;

    /// The next pickups from today onwards, earliest first
    /// (at most five).
    std::vector<UpcomingPickup> upcoming;

    /// Live bookings whose end date is already past — rentals
    /// the staff should chase up before invoicing.
    int overdueReturns = 0;

    /// All queued bookings fleet-wide (same number the
    /// statistics screen reports).
    int totalActiveBookings = 0;

    /// Customers waiting for a free slot fleet-wide (v2.6.0).
    int totalWaitlisted = 0;
};

/// Compute the dashboard numbers for `mgr` as of `todaySerial`.
AnalyticsReport buildAnalytics(const RentalManager &mgr, int todaySerial);

/// Render one proportional bar: `count` out of `maxCount` over
/// `width` cells of U+2588, rounded to nearest, at least one
/// block for a non-zero count and empty for zero. Pure and
/// deterministic — the tests pin the exact outputs.
std::string analyticsBar(int count, int maxCount, int width = 24);

/// Right-align `text` in a field of `width` characters.
std::string analyticsPad(const std::string &text, std::size_t width);

#endif // ANALYTICS_H
