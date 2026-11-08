#ifndef USER_H
#define USER_H

#include <string>

#include "SHA256.h"

/**
 * ============================================================
 *  User / Customer / Staff / Manager
 * ============================================================
 *  Role hierarchy of the rental system.
 *
 *  Security model:
 *  - Passwords are NEVER stored in plain text. Each user gets
 *    a random 8-byte salt; the stored value is
 *        SHA256(salt + password)
 *    so identical passwords still produce different digests.
 *  - After 3 failed login attempts the account is blocked and
 *    must be re-enabled by a Manager.
 *
 *  Class roles:
 *    Customer -> has a monetary balance (rental charges and
 *                late penalties accumulate here).
 *    Staff    -> front-desk operations (deliver / return /
 *                maintenance). Behavior enforced by the CLI.
 *    Manager  -> fleet and account administration.
 * ============================================================
 */
class User
{
public:
    User(const std::string &username, const std::string &role);
    virtual ~User() = default;

    // Users are owned by the HashTable; copying is disabled.
    User(const User &) = delete;
    User &operator=(const User &) = delete;

    const std::string &username() const { return username_; }
    virtual std::string role() const = 0; // polymorphic role tag
    bool isBlocked() const { return blocked_; }

    /* ---------------- Password handling ---------------- */

    /// Hash + store a new password with a fresh random salt.
    void setPassword(const std::string &plainPassword);

    /// Constant-shape comparison against the stored digest.
    bool verifyPassword(const std::string &plainPassword) const;

    /* ---------------- Persistence (CSV round-trip) ---------------- */

    std::string salt() const { return salt_; }
    std::string passwordHash() const { return passwordHash_; }

    /// Rebuild internal state from a saved users.csv row.
    void restoreCredentials(const std::string &salt, const std::string &hash,
                            bool blocked, int failedLogins)
    {
        salt_ = salt;
        passwordHash_ = hash;
        blocked_ = blocked;
        failedLogins_ = failedLogins;
    }

    /* ---------------- Account state ---------------- */

    void block() { blocked_ = true; }
    void unblock()
    {
        blocked_ = false;
        failedLogins_ = 0;
    }

    int failedLogins() const { return failedLogins_; }
    void registerFailedLogin()
    {
        ++failedLogins_;
        if (failedLogins_ >= 3)
            blocked_ = true;
    }
    void resetFailedLogins() { failedLogins_ = 0; }

    /// Serialized form used by data/users.csv.
    virtual std::string serializeExtra() const { return ""; }

protected:
    std::string username_;
    std::string role_;
    std::string salt_;
    std::string passwordHash_;
    bool blocked_;
    int failedLogins_;
};

class Customer : public User
{
public:
    Customer(const std::string &username)
        : User(username, "Customer"), balance_(0.0), loyaltyPoints_(0),
          lifetimePoints_(0), completedRentals_(0)
    {
    }

    std::string role() const override { return "Customer"; }

    /// Amount the customer still owes (rentals + late penalties).
    double balance() const { return balance_; }
    void charge(double amount) { balance_ += amount; }
    void payBalance() { balance_ = 0.0; }
    void setBalance(double amount) { balance_ = amount; }

    /// Apply a loyalty redemption credit to the bill (clamped at 0,
    /// so a rounding hiccup can never make the balance negative).
    void creditBill(double amount)
    {
        balance_ -= amount;
        if (balance_ < 0.0)
            balance_ = 0.0;
    }

    /* ---------------- Loyalty program (v2.7.0) ----------------
     *  Rules live in Loyalty.h; this record only stores the three
     *  counters. The tier is always derived from lifetimePoints_.
     */

    /// Points available for redemption (lifetime minus redeemed).
    int loyaltyPoints() const { return loyaltyPoints_; }
    /// Never decremented — decides the tier.
    int lifetimePoints() const { return lifetimePoints_; }
    /// Returns processed for this customer so far.
    int completedRentals() const { return completedRentals_; }

    /// Earn path (facade, after a completed rental): credit points.
    /// Non-positive requests are ignored, so a zero invoice is a no-op.
    void awardLoyaltyPoints(int points)
    {
        if (points <= 0)
            return;
        loyaltyPoints_ += points;
        lifetimePoints_ += points;
    }

    /// Redeem path (facade, after validation): spend points.
    void deductLoyaltyPoints(int points) { loyaltyPoints_ -= points; }

    /// Bookkeeping for the completed-rentals counter.
    void recordCompletedRental() { ++completedRentals_; }

    /// Restore a saved users.csv row. Legacy rows (pre-2.7.0) carry
    /// no loyalty columns and default to all-zero.
    void restoreLoyalty(int points, int lifetime, int completedRentals)
    {
        loyaltyPoints_ = points;
        lifetimePoints_ = lifetime;
        completedRentals_ = completedRentals;
    }

    std::string serializeExtra() const override;

private:
    double balance_;
    int loyaltyPoints_;
    int lifetimePoints_;
    int completedRentals_;
};

class Staff : public User
{
public:
    Staff(const std::string &username) : User(username, "Staff") {}
    std::string role() const override { return "Staff"; }
};

class Manager : public User
{
public:
    Manager(const std::string &username) : User(username, "Manager") {}
    std::string role() const override { return "Manager"; }
};

#endif // USER_H
