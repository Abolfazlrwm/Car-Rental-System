#ifndef RECURRING_H
#define RECURRING_H

#include <string>
#include <vector>

#include "Date.h"

/**
 * ============================================================
 *  Recurring — the repeat-booking rule engine   (v3.0.0)
 * ============================================================
 *  Pure rules for series bookings, no I/O and no mutable state:
 *  every function here deterministically answers one question,
 *  so the facade, the CLI and the tests all read the same truth
 *  (same layering as Loyalty, v2.7.0).
 *
 *  How a series works:
 *    - the customer books ONE car for a range and chooses a
 *      plan (weekly / biweekly / monthly) and how many bookings
 *      the series holds;
 *    - every occurrence repeats the SAME duration on the same
 *      car: the next pickup lands one plan-step after the
 *      previous PICKUP (weekly = +7 days, biweekly = +14,
 *      monthly = one calendar month, day-of-month clamped);
 *    - the whole series is created atomically — if any
 *      occurrence would clash with an existing booking or with
 *      another occurrence of the same series, nothing is booked
 *      and the caller sees the reason.
 * ============================================================
 */

enum class RecurrencePlan
{
    Weekly,    // every 7 days
    Biweekly,  // every 14 days
    Monthly    // one calendar month later (clamped day-of-month)
};

/// Human-readable plan name (CLI screens, audit details, tests).
inline const char *toString(RecurrencePlan plan)
{
    switch (plan)
    {
    case RecurrencePlan::Weekly:
        return "weekly";
    case RecurrencePlan::Biweekly:
        return "biweekly";
    case RecurrencePlan::Monthly:
        return "monthly";
    }
    return "weekly";
}

/// CLI choice -> plan (0 is handled by the caller as "one-off").
inline bool planFromChoice(int choice, RecurrencePlan &planOut)
{
    switch (choice)
    {
    case 1:
        planOut = RecurrencePlan::Weekly;
        return true;
    case 2:
        planOut = RecurrencePlan::Biweekly;
        return true;
    case 3:
        planOut = RecurrencePlan::Monthly;
        return true;
    }
    return false;
}

/// Upper bound on bookings per series — keeps the atomic check
/// (and the menu output) bounded.
constexpr int MAX_SERIES_OCCURRENCES = 8;

/// Smallest series the facade accepts (a "series" of one is just
/// a normal reservation).
constexpr int MIN_SERIES_OCCURRENCES = 2;

/// The pickup day of `occurrenceIndex` (0-based) of a series:
/// `firstStart` for index 0, then one plan-step later each time.
/// Weekly/biweekly add fixed day counts; monthly re-derives every
/// pickup from the FIRST start (one calendar month per index), so a
/// Jan-31 anchor lands on Feb-28, Mar-31, Apr-30... — the day of
/// month stays anchored, it is never re-based onto the clamped day.
inline int occurrenceStart(RecurrencePlan plan, int firstStart,
                           int occurrenceIndex)
{
    switch (plan)
    {
    case RecurrencePlan::Weekly:
        return firstStart + 7 * occurrenceIndex;
    case RecurrencePlan::Biweekly:
        return firstStart + 14 * occurrenceIndex;
    case RecurrencePlan::Monthly:
        return Date::addMonths(firstStart, occurrenceIndex);
    }
    return firstStart;
}

/**
 *  Build every pickup day of a series. Returns false — and
 *  leaves `startsOut` untouched — when the series is impossible:
 *    - fewer than MIN or more than MAX occurrences;
 *    - an invalid first date (before the 1970 booking floor);
 *    - one occurrence would still be running when the next one
 *      starts (duration longer than the plan step — the series
 *      would collide with itself).
 *  On success, `startsOut` holds exactly `occurrences` ascending
 *  pickup days and consecutive ranges never overlap.
 */
inline bool buildSeriesStarts(RecurrencePlan plan, int firstStart,
                              int durationDays, int occurrences,
                              std::vector<int> &startsOut)
{
    if (occurrences < MIN_SERIES_OCCURRENCES ||
        occurrences > MAX_SERIES_OCCURRENCES)
        return false;
    if (firstStart < Date::MIN_SERIAL || durationDays < 1)
        return false;

    std::vector<int> starts;
    starts.reserve(static_cast<std::size_t>(occurrences));
    for (int i = 0; i < occurrences; ++i)
    {
        const int start = occurrenceStart(plan, firstStart, i);
        if (start < Date::MIN_SERIAL)
            return false;
        if (i > 0 && start <= starts[static_cast<std::size_t>(i - 1)] +
                                 durationDays - 1)
            return false; // self-overlap: previous rental is still out
        starts.push_back(start);
    }

    startsOut.swap(starts);
    return true;
}

#endif // RECURRING_H
