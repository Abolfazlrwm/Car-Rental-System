#ifndef PRICING_H
#define PRICING_H

#include <iomanip>
#include <sstream>
#include <string>

#include "Loyalty.h"

/**
 * ============================================================
 *  Pricing — the dynamic discount engine   (v3.0.0)
 * ============================================================
 *  Pure rules of the invoice, no I/O and no mutable state: every
 *  function here deterministically answers one pricing question,
 *  so the facade, the CLI and the tests all read the same truth
 *  (same layering as Loyalty and Recurring).
 *
 *  How an invoice is priced:
 *    - BASE      = booked days x daily rate (inclusive day count);
 *    - LONG RENTAL: the booked duration earns a discount rate —
 *                  3+ days 3%, 7+ days 7%, 14+ days 12%,
 *                  30+ days 18%;
 *    - RANK      : the loyalty rank held at return time earns
 *                  another rate — Silver 3%, Gold 6%,
 *                  Platinum 10%;
 *    - the two rates STACK MULTIPLICATIVELY
 *                  (effective = (1-duration) x (1-tier)), so a
 *                  7% + 3% stack takes 9.79% off, never 10% —
 *                  discounts can never compound past 100%;
 *    - LATE DAYS : every late day costs the RAW daily rate x the
 *                  penalty multiplier — a discount never softens
 *                  a penalty;
 *    - TOTAL     = base - discount + penalty.
 *
 *  The same engine prices the booking-time estimate (rank held
 *  today) and the return invoice (rank held on the return day) —
 *  one computation, two moments, so screen and invoice can never
 *  drift apart.
 * ============================================================
 */

/* ---------------- Rate tables ---------------- */

constexpr double LONG_RENTAL_DISCOUNT_3_PLUS  = 0.03;
constexpr double LONG_RENTAL_DISCOUNT_7_PLUS  = 0.07;
constexpr double LONG_RENTAL_DISCOUNT_14_PLUS = 0.12;
constexpr double LONG_RENTAL_DISCOUNT_30_PLUS = 0.18;

constexpr double TIER_DISCOUNT_SILVER   = 0.03;
constexpr double TIER_DISCOUNT_GOLD     = 0.06;
constexpr double TIER_DISCOUNT_PLATINUM = 0.10;

/// Late-day multiplier — the v1.x penalty rule, now owned by the
/// pricing layer so the invoice is priced in one place.
constexpr double LATE_PENALTY_RATE = 1.5;

/// The long-rental rate for a booked duration (inclusive days).
inline double durationDiscount(int days)
{
    if (days >= 30)
        return LONG_RENTAL_DISCOUNT_30_PLUS;
    if (days >= 14)
        return LONG_RENTAL_DISCOUNT_14_PLUS;
    if (days >= 7)
        return LONG_RENTAL_DISCOUNT_7_PLUS;
    if (days >= 3)
        return LONG_RENTAL_DISCOUNT_3_PLUS;
    return 0.0;
}

/// The rank rate for the tier held when the invoice is priced.
inline double tierDiscount(LoyaltyTier tier)
{
    switch (tier)
    {
    case LoyaltyTier::Silver:
        return TIER_DISCOUNT_SILVER;
    case LoyaltyTier::Gold:
        return TIER_DISCOUNT_GOLD;
    case LoyaltyTier::Platinum:
        return TIER_DISCOUNT_PLATINUM;
    case LoyaltyTier::Bronze:
        break;
    }
    return 0.0;
}

/**
 *  Every number of one invoice, in the order the desk reads them:
 *  base, the two rates, the discount amount, the late penalty and
 *  the total actually charged. A degenerate request (no days or a
 *  negative rate) prices to zero instead of inventing money.
 */
struct PriceBreakdown
{
    double base = 0.0;          // days x pricePerDay
    double durationRate = 0.0;  // 0..0.18 long-rental rate
    double tierRate = 0.0;      // 0..0.10 rank rate
    LoyaltyTier tier = LoyaltyTier::Bronze; // rank the invoice was priced at
    double discount = 0.0;      // amount off the base
    double penalty = 0.0;       // late fees, never discounted
    double total = 0.0;         // base - discount + penalty

    /// 0..100, the stacked percentage actually taken off the base.
    double effectiveRate() const
    {
        return base > 0.0 ? discount / base : 0.0;
    }
};

/// Price one rental: `durationDays` booked days at `pricePerDay`,
/// for a customer of rank `tier`, with `lateDays` late days.
inline PriceBreakdown quoteRental(int durationDays, double pricePerDay,
                                  LoyaltyTier tier, int lateDays = 0)
{
    PriceBreakdown q;
    if (durationDays < 1 || pricePerDay < 0.0)
        return q;

    q.base = durationDays * pricePerDay;
    q.durationRate = durationDiscount(durationDays);
    q.tierRate = tierDiscount(tier);
    q.tier = tier;
    const double effective = (1.0 - q.durationRate) * (1.0 - q.tierRate);
    q.discount = q.base * (1.0 - effective);
    q.penalty = lateDays > 0 ? lateDays * pricePerDay * LATE_PENALTY_RATE
                             : 0.0;
    q.total = q.base - q.discount + q.penalty;
    return q;
}

/// Human-readable discount note for the desk and the audit trail:
/// "27.00 off (3% long rental)", "56.70 off (12% long rental,
/// 3% Gold)" — empty when nothing was discounted.
inline std::string discountNote(const PriceBreakdown &q)
{
    if (q.discount <= 0.0)
        return "";

    std::ostringstream parts;
    if (q.durationRate > 0.0)
        parts << static_cast<int>(q.durationRate * 100.0 + 0.5)
              << "% long rental";
    if (q.tierRate > 0.0)
    {
        if (!parts.str().empty())
            parts << ", ";
        parts << static_cast<int>(q.tierRate * 100.0 + 0.5) << "% "
              << toString(q.tier);
    }

    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << q.discount << " off ("
        << parts.str() << ")";
    return out.str();
}

#endif // PRICING_H
