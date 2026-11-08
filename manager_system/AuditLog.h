#ifndef AUDIT_LOG_H
#define AUDIT_LOG_H

#include <string>
#include <vector>

#include "../basic_structures/DoublyLinkedList.h"

/**
 * ============================================================
 *  AuditEntry / AuditLog   (v2.5.0)
 * ============================================================
 *  Append-only trail of privileged actions: who did what, when,
 *  and whether the permission matrix allowed it.
 *
 *  The facade (RentalManager) records entries itself, so feature
 *  code cannot forget to log; callers only READ the trail.
 *
 *  Storage: the project's own DoublyLinkedList (O(1) append at
 *  the tail = newest entry), round-tripped through
 *  data/audit_log.csv with proper RFC-4180 quoting, so details
 *  may safely contain commas, quotes and newlines.
 * ============================================================
 */
struct AuditEntry
{
    std::string date;   // ISO YYYY-MM-DD (the 2.3.0 calendar layer)
    std::string actor;  // username, "?" when no session was active
    std::string action; // dotted action id, e.g. "login.failed"
    std::string detail; // human-readable context
    bool allowed;       // false = denied / failed attempt
};

class AuditLog
{
public:
    /// Append one entry (newest goes to the tail).
    void record(const std::string &date, const std::string &actor,
                const std::string &action, const std::string &detail,
                bool allowed)
    {
        entries_.pushBack(AuditEntry{date, actor, action, detail, allowed});
    }

    bool empty() const { return entries_.empty(); }
    std::size_t size() const { return entries_.size(); }

    /// Every entry, oldest first.
    std::vector<AuditEntry> all() const;

    /// The newest `n` entries still in chronological order
    /// (fewer when the trail is shorter, empty when n == 0).
    std::vector<AuditEntry> recent(std::size_t n) const;

    /* ---------------- CSV round-trip ---------------- */

    /// Write header + one row per entry to `path`. Fields containing
    /// commas, quotes or newlines are RFC-4180 quoted.
    bool saveCSV(const std::string &path) const;

    /// Replace the in-memory trail with the file's contents.
    /// Returns false when the file cannot be opened (a missing file
    /// is normal for fresh installs and simply keeps the trail empty).
    bool loadCSV(const std::string &path);

    /// RFC-4180 field quoting (exposed for unit tests).
    static std::string escapeField(const std::string &raw);

private:
    DoublyLinkedList<AuditEntry> entries_; // chronological, back = newest
};

#endif // AUDIT_LOG_H
