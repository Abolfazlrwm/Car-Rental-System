#include "Date.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>

/* 1970-01-01 == serial day 0 (the Unix epoch, the booking floor). */
const int Date::MIN_SERIAL = 0;

namespace
{
/* ============================================================
 *  Proleptic Gregorian calendar <-> serial day conversions.
 *  Based on Howard Hinnant's public-domain civil-date
 *  algorithms — the same math std::chrono builds on, minus the
 *  C++20 requirement (this project stays C++17 and
 *  dependency-free). Valid for the whole year range 1..9999.
 * ============================================================ */

/// Days since 1970-01-01 (negative before the epoch).
int daysFromCivil(int y, unsigned m, unsigned d)
{
    y -= (m <= 2);
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);      // [0, 399]
    const unsigned doy =
        (153u * (m + (m > 2 ? -3u : 9u)) + 2u) / 5u + d - 1;        // [0, 365]
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;  // [0, 146096]
    return era * 146097 + static_cast<int>(doe) - 719468;
}

/// Inverse of daysFromCivil.
void civilFromDays(int z, int &yOut, unsigned &mOut, unsigned &dOut)
{
    z += 719468;
    const int era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);   // [0, 146096]
    const unsigned yoe =
        (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;  // [0, 399]
    const int y = static_cast<int>(yoe) + era * 400;
    const unsigned doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    const unsigned mp = (5u * doy + 2u) / 153u;                     // [0, 11]
    dOut = doy - (153u * mp + 2u) / 5u + 1u;                        // [1, 31]
    mOut = (mp < 10u) ? mp + 3u : mp - 9u;                          // [1, 12]
    yOut = y + ((mOut <= 2) ? 1 : 0);
}

bool isLeapYear(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

unsigned daysInMonth(int y, unsigned m)
{
    static const unsigned kDays[12] = {31, 28, 31, 30, 31, 30,
                                       31, 31, 30, 31, 30, 31};
    if (m == 2 && isLeapYear(y))
        return 29;
    return kDays[m - 1];
}
} // namespace

bool Date::isValid(int year, int month, int day)
{
    if (year < 1 || year > 9999 || month < 1 || month > 12)
        return false;
    return day >= 1 && day <= static_cast<int>(daysInMonth(year,
                                                           static_cast<unsigned>(month)));
}

bool Date::parseISO(const std::string &text, int &serialOut)
{
    // ---- strict shape: ####-##-## ------------------------------------
    if (text.size() != 10 || text[4] != '-' || text[7] != '-')
        return false;
    static const int kDigitPos[8] = {0, 1, 2, 3, 5, 6, 8, 9};
    for (int pos : kDigitPos)
        if (!std::isdigit(static_cast<unsigned char>(text[pos])))
            return false;

    // ---- strict calendar validity (leap years included) --------------
    const int y = std::stoi(text.substr(0, 4));
    const int m = std::stoi(text.substr(5, 2));
    const int d = std::stoi(text.substr(8, 2));
    if (!isValid(y, m, d))
        return false;

    const int serial = daysFromCivil(y, static_cast<unsigned>(m),
                                     static_cast<unsigned>(d));
    if (serial < MIN_SERIAL) // business floor: nothing before 1970-01-01
        return false;

    serialOut = serial;
    return true;
}

std::string Date::toISO(int serialDay)
{
    int y = 0;
    unsigned m = 0, d = 0;
    civilFromDays(serialDay, y, m, d);
    char buf[32]; // roomy: silences -Wformat-truncation on odd serials
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02u", y, m, d);
    return std::string(buf);
}

bool Date::isISODate(const std::string &text)
{
    int serial = 0;
    return parseISO(text, serial);
}

bool Date::parseFlexible(const std::string &text, int &serialOut)
{
    // New format first.
    if (parseISO(text, serialOut))
        return true;

    // Legacy fallback: a bare day number (pre-2.3.0 data files).
    if (text.empty())
        return false;
    char *endp = nullptr;
    const long value = std::strtol(text.c_str(), &endp, 10);
    if (endp != text.c_str() + text.size() || value < MIN_SERIAL)
        return false;

    serialOut = static_cast<int>(value);
    return true;
}
