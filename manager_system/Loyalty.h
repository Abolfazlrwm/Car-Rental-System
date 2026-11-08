#ifndef LOYALTY_H
#define LOYALTY_H

#include <string>

/**
 * ============================================================
 *  Loyalty — the frequent-renter tier engine   (v2.7.0)
 * ============================================================
 *  Pure rules of the loyalty program, no I/O and no mutable
 *  state: every function here deterministically answers one
 *  program question, so the facade, the CLI and the tests all
 *  read the same truth (same layering as Analytics, v2.4.0).
 *
 *  How the program works:
 *    - Completing a rental (return processed, invoice written)
 *      pays  floor(spend * POINTS_PER_CURRENCY * tier multiplier)
 *      points, where the multiplier comes from the tier the
 *      customer ALREADY holds — ranks grow from history, not
 *      from the invoice being written right now.
 *    - Lifetime points are never decremented and decide the
 *      tier; available points are spent on bill credits.
 *    - Redemption: 10 points = 1.00 off the outstanding bill,
 *      minimum 50 points, whole currency only, and never more
 *      than the bill itself — points can only be spent, not
 *      converted to cash, so nothing is ever wasted.
 *
 *  The tier is DERIVED from lifetime points on every read
 *  (tierForLifetimePoints), not stored — there is no extra
 *  state to forget when a save/load round-trip happens.
 * ============================================================
 */

enum class LoyaltyTier
{
    Bronze,
    Silver,
    Gold,
    Platinum
};

/// Human-readable tier name (CLI screens, audit details, tests).
inline const char *toString(LoyaltyTier tier)
{
    switch (tier)
    {
    case LoyaltyTier::Bronze:
        return "Bronze";
    case LoyaltyTier::Silver:
        return "Silver";
    case LoyaltyTier::Gold:
        return "Gold";
    case LoyaltyTier::Platinum:
        return "Platinum";
    }
    return "Bronze";
}

/// Lifetime-point count that unlocks `tier` (Bronze = open door).
inline int tierThreshold(LoyaltyTier tier)
{
    switch (tier)
    {
    case LoyaltyTier::Bronze:
        return 0;
    case LoyaltyTier::Silver:
        return 250;
    case LoyaltyTier::Gold:
        return 1000;
    case LoyaltyTier::Platinum:
        return 2500;
    }
    return 0;
}

/// Earning factor a tier applies to every completed rental.
inline double tierMultiplier(LoyaltyTier tier)
{
    switch (tier)
    {
    case LoyaltyTier::Bronze:
        return 1.0;
    case LoyaltyTier::Silver:
        return 1.25;
    case LoyaltyTier::Gold:
        return 1.5;
    case LoyaltyTier::Platinum:
        return 2.0;
    }
    return 1.0;
}

/// The tier for a lifetime-point balance. Derived on every read,
/// so a customer crosses a tier the moment the threshold is met.
inline LoyaltyTier tierForLifetimePoints(int lifetimePoints)
{
    if (lifetimePoints >= tierThreshold(LoyaltyTier::Platinum))
        return LoyaltyTier::Platinum;
    if (lifetimePoints >= tierThreshold(LoyaltyTier::Gold))
        return LoyaltyTier::Gold;
    if (lifetimePoints >= tierThreshold(LoyaltyTier::Silver))
        return LoyaltyTier::Silver;
    return LoyaltyTier::Bronze;
}

/// The rank right above `tier`; Platinum maps to itself (top rank).
inline LoyaltyTier nextTier(LoyaltyTier tier)
{
    if (tier == LoyaltyTier::Bronze)
        return LoyaltyTier::Silver;
    if (tier == LoyaltyTier::Silver)
        return LoyaltyTier::Gold;
    return LoyaltyTier::Platinum;
}

/// Points still missing for the next rank (0 at Platinum).
inline int pointsToNextTier(int lifetimePoints)
{
    const LoyaltyTier current = tierForLifetimePoints(lifetimePoints);
    if (current == LoyaltyTier::Platinum)
        return 0;
    return tierThreshold(nextTier(current)) - lifetimePoints;
}

/* ---------------- Program constants ---------------- */

/// Points paid per 1.00 of invoiced spend (1 point per 10.00).
constexpr double POINTS_PER_CURRENCY = 0.1;

/// Credit value of one point at redemption (10 points = 1.00).
constexpr double POINT_VALUE = 0.1;

/// Redemption happens in steps of 10 points so a credit is a
/// whole-currency amount — no rounding disputes at the desk.
constexpr int REDEEM_STEP = 10;

/// Smallest redemption the program accepts.
constexpr int MIN_REDEEM_POINTS = 50;

/// Points awarded for one completed rental worth `spend`:
/// 1 point per 10.00, multiplied by the CURRENT tier's factor,
/// floored to whole points. A zero/negative invoice earns nothing.
inline int pointsEarnedFor(int lifetimePoints, double spend)
{
    if (spend <= 0.0)
        return 0;
    const double raw = spend * POINTS_PER_CURRENCY *
                       tierMultiplier(tierForLifetimePoints(lifetimePoints));
    return static_cast<int>(raw); // floor: raw is never negative here
}

/// Credit value of `points` when redeemed against a bill.
inline double pointsToValue(int points) { return points * POINT_VALUE; }

/// Is `points` a well-formed redemption? At least MIN_REDEEM_POINTS
/// and a multiple of REDEEM_STEP, so the credit is whole currency.
inline bool isRedeemableAmount(int points)
{
    return points >= MIN_REDEEM_POINTS && points % REDEEM_STEP == 0;
}

/**
 *  Snapshot of one customer's standing. The facade builds it
 *  from the Customer record; the CLI renders it; the tests pin
 *  the numbers. points can be spent, lifetimePoints cannot.
 */
struct LoyaltyProfile
{
    int points = 0;          // spendable points right now
    int lifetimePoints = 0;  // earned over all time, never spent
    int completedRentals = 0;

    LoyaltyTier tier() const { return tierForLifetimePoints(lifetimePoints); }
    /// 0 when the top rank is reached.
    int pointsToNextTier() const
    {
        return ::pointsToNextTier(lifetimePoints);
    }
};

/**
 *  Fleet-wide loyalty snapshot for the manager screen: member
 *  counts per tier (fixed Bronze..Platinum order, indexable by
 *  the enum), the points currently held by customers and what
 *  those points could still cost the business as bill credits.
 */
struct LoyaltyOverview
{
    int membersByTier[4] = {0, 0, 0, 0}; // [Bronze, Silver, Gold, Platinum]
    int totalMembers = 0;
    int outstandingPoints = 0;    // spendable points held right now
    double pointsLiability = 0.0; // outstandingPoints as credit value
    std::string topMember;        // most lifetime points, name breaks ties
    int topMemberLifetime = 0;
};

#endif // LOYALTY_H
