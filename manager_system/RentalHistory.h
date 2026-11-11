#ifndef RENTAL_HISTORY_H
#define RENTAL_HISTORY_H

#include <functional>
#include <string>
#include <vector>

/**
 * ============================================================
 *  RentalHistory — the completed-rentals ledger   (v3.0.0)
 * ============================================================
 *  Until v3.0.0 a finished rental vanished: the booking left the
 *  schedule, the invoice melted into the per-car income ledger,
 *  and no report could ever answer "how much did November make?".
 *
 *  The ledger fixes that: every processed return appends ONE
 *  record with the full invoice breakdown (base, discount,
 *  penalty, total) plus the car, the customer and the three
 *  calendar dates. It is append-only, round-trips through
 *  data/rental_history.csv, and feeds the manager's monthly
 *  report (MonthlyReport.h).
 *
 *  The ledger stores facts, not behaviour: it never recomputes
 *  anything and never decides anything — pricing lives in
 *  Pricing.h, aggregation lives in MonthlyReport.h.
 * ============================================================
 */

/// One completed rental, exactly as it was invoiced.
struct RentalRecord
{
    std::string carName;
    std::string customer;
    std::string startISO;     // booked pickup, "YYYY-MM-DD"
    std::string endISO;       // booked return, "YYYY-MM-DD"
    std::string returnISO;    // ACTUAL return day — the month bucket
    double base = 0.0;        // duration x daily rate
    double discount = 0.0;    // amount taken off the base (v3.0.0 rates)
    double penalty = 0.0;     // late fees, never discounted
    double total = 0.0;       // base - discount + penalty

    RentalRecord() = default;
    RentalRecord(std::string car, std::string customerName,
                 std::string start, std::string end, std::string ret,
                 double b, double d, double p, double t)
        : carName(std::move(car)), customer(std::move(customerName)),
          startISO(std::move(start)), endISO(std::move(end)),
          returnISO(std::move(ret)), base(b), discount(d), penalty(p),
          total(t) {}
};

class RentalHistory
{
public:
    /// Append one completed rental (append-only: no update, no erase).
    void add(const RentalRecord &record) { records_.push_back(record); }

    /// Drop the whole ledger (state reload).
    void clear() { records_.clear(); }

    std::size_t size() const { return records_.size(); }
    bool empty() const { return records_.empty(); }

    /// Visit every record in completion order (oldest first).
    void forEach(
        const std::function<void(const RentalRecord &)> &fn) const
    {
        for (const RentalRecord &r : records_)
            fn(r);
    }

    /// Write the ledger to `path` as CSV (header + one row per
    /// record, in completion order). Missing parent directories are
    /// created; false when the file cannot be opened for writing.
    bool saveCSV(const std::string &path) const;

    /// Load a previously saved ledger. A missing file is a fresh
    /// install, not an error (the ledger stays empty); corrupt rows
    /// are dropped instead of crashing the loader.
    bool loadCSV(const std::string &path);

private:
    std::vector<RentalRecord> records_; // oldest first
};

#endif // RENTAL_HISTORY_H
