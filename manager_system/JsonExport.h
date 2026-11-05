#ifndef JSON_EXPORT_H
#define JSON_EXPORT_H

#include <string>

class RentalManager;

/**
 * ============================================================
 *  JSON report export
 * ============================================================
 *  Writes the whole system state to a single `report.json`:
 *
 *    stats     — the FleetStats snapshot (same numbers as the
 *                manager's statistics screen, so the two can
 *                never drift apart)
 *    fleet     — every car, name-sorted, with its maintenance
 *                log and revenue figures
 *    bookings  — every live reservation, ordered by pickup
 *                date (id breaks ties), dates as ISO strings
 *
 *  Zero dependencies: the writer is a handful of string
 *  builders behind a strict escaper (quotes, backslashes and
 *  control characters per RFC 8259), and the unit suite
 *  validates the produced file with its own tiny
 *  recursive-descent JSON parser — the output is not just
 *  "looks right", it is proven parseable.
 * ============================================================
 */

/** Escape a UTF-8 string for use inside a JSON string literal
 *  (RFC 8259): `"` and `\` get a backslash, control characters
 *  become \b \f \n \r \t or \u00XX; multi-byte UTF-8 (e.g.
 *  non-ASCII names) passes through untouched. */
std::string jsonEscape(const std::string &text);

/// Write the combined report to `path`. Missing parent
/// directories are created; returns false when the file cannot
/// be opened for writing.
bool exportJsonReport(const RentalManager &manager, const std::string &path);

#endif // JSON_EXPORT_H
