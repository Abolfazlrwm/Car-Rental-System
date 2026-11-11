#ifndef DATE_H
#define DATE_H

#include <string>

/**
 * ============================================================
 *  Date
 * ============================================================
 *  Calendar helper bridging human-readable ISO dates
 *  ("YYYY-MM-DD") and the serial day numbers the scheduler
 *  works with internally (days since 1970-01-01 — the Unix
 *  epoch, which is also the booking floor).
 *
 *  Why a thin converter instead of rewriting the core?
 *    - the scheduling math (overlap, duration, late penalties)
 *      is integer arithmetic on serial days and is already
 *      covered by the regression suite;
 *    - only the UI boundary (parsing what the user types) and
 *      the CSV boundary (what gets stored) need calendar
 *      awareness, so that is all this class does.
 *
 *  Parsing is strict: exactly 10 characters "YYYY-MM-DD",
 *  digits only, and a valid calendar date on the proleptic
 *  Gregorian calendar — full leap-year handling (2024-02-29
 *  parses; 1900-02-29 and 2026-02-30 do not). The conversions
 *  use Howard Hinnant's public-domain civil-date algorithms,
 *  keeping the project C++17 and dependency-free.
 * ============================================================
 */
class Date
{
public:
    /// Earliest bookable day: 1970-01-01 (serial day 0).
    static const int MIN_SERIAL;

    /// Parse a strict "YYYY-MM-DD" string into a serial day
    /// (days since 1970-01-01). False on any shape or calendar
    /// deviation, or dates before MIN_SERIAL.
    static bool parseISO(const std::string &text, int &serialOut);

    /// Serial day -> "YYYY-MM-DD" (always 10 chars, zero-padded).
    /// Lexicographic order on the result matches chronological
    /// order on the serials.
    static std::string toISO(int serialDay);

    /// Calendar validity check for a year/month/day triple
    /// (leap years included). Range: years 1..9999.
    static bool isValid(int year, int month, int day);

    /// True when the text is a well-formed ISO date.
    static bool isISODate(const std::string &text);

    /// Load-time tolerance: accept a strict ISO date OR a bare
    /// day number, so CSV files written by pre-2.3.0 builds
    /// (which stored day numbers) still load.
    static bool parseFlexible(const std::string &text, int &serialOut);

    /// `months` calendar months after `serial` (months >= 0), keeping
    /// the day-of-month. A day past the target month's length clamps
    /// to its last day (2026-01-31 + 1 month = 2026-02-28), so a
    /// monthly recurrence never lands on a phantom date. Pure
    /// arithmetic — the caller validates the booking floor.
    static int addMonths(int serial, int months);

    /// Today's serial day, read from the system clock (local
    /// timezone). Powers the "generatedAt" stamp of the JSON
    /// report and the analytics dashboard's "days away" math.
    static int todaySerial();
};

#endif // DATE_H
