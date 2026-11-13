/**
 * ============================================================
 *  Car Rental Management System — Console Application
 * ============================================================
 *  Entry point: runs the interactive CLI on top of RentalManager.
 *
 *  Robustness rules enforced here (all missing in the original):
 *    - every prompt uses getline-based helpers, so input never
 *      desynchronizes and values with SPACES are accepted;
 *    - numeric prompts re-ask until the value is a valid number;
 *    - EOF / broken input terminates the loop cleanly instead
 *      of spinning forever.
 *
 *  Colors are ANSI-based and automatically disabled when the
 *  output is not a terminal (piping / redirection / CI logs).
 * ============================================================
 */

#include <cstdio>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <io.h>
#define IS_TTY _isatty(_fileno(stdout))
#else
#include <unistd.h>
#define IS_TTY isatty(STDOUT_FILENO)
#endif

#include "manager_system/Analytics.h"
#include "manager_system/RentalManager.h"
#include "manager_system/Date.h"
#include "manager_system/JsonExport.h"
#include "manager_system/Loyalty.h"
#include "manager_system/MonthlyReport.h"
#include "manager_system/Pricing.h"
#include "manager_system/Recurring.h"

/* Program version, reported by --version / --help. */
static const char *APP_VERSION = "3.1.0";

/* ============================================================
 *  Terminal colors (no-ops when stdout is piped)
 * ============================================================ */

static const bool g_colors = IS_TTY;

static std::string colorize(const std::string &text, const char *code)
{
    return g_colors ? std::string(code) + text + "\033[0m" : text;
}

static std::string bold(const std::string &s) { return colorize(s, "\033[1m"); }
static std::string green(const std::string &s) { return colorize(s, "\033[32m"); }
static std::string red(const std::string &s) { return colorize(s, "\033[31m"); }
static std::string yellow(const std::string &s) { return colorize(s, "\033[33m"); }
static std::string cyan(const std::string &s) { return colorize(s, "\033[36m"); }

/* ============================================================
 *  Input helpers — the anti-segfault, anti-infinite-loop layer
 * ============================================================ */

/// Read one line; returns false on EOF (input exhausted).
static bool readLine(const std::string &prompt, std::string &out)
{
    std::cout << prompt;
    std::cout.flush();
    if (!std::getline(std::cin, out))
        return false; // EOF -> caller exits gracefully
    while (!out.empty() && (out.back() == '\r' || out.back() == ' '))
        out.pop_back();
    return true;
}

/// Read a trimmed non-empty line.
static bool readToken(const std::string &prompt, std::string &out)
{
    std::string line;
    do
    {
        if (!readLine(prompt, line))
            return false;
    } while (line.empty());
    out = line;
    return true;
}

/// Read an integer, re-asking until the input parses. Returns false on EOF.
static bool readInt(const std::string &prompt, int &out)
{
    std::string line;
    while (readToken(prompt, line))
    {
        try
        {
            std::size_t consumed = 0;
            int value = std::stoi(line, &consumed);
            if (consumed == line.size())
            {
                out = value;
                return true;
            }
        }
        catch (...)
        {
        }
        std::cout << red("  Please enter a whole number.\n");
    }
    return false;
}

/// Read a non-negative double, re-asking until the input parses.
static bool readDouble(const std::string &prompt, double &out)
{
    std::string line;
    while (readToken(prompt, line))
    {
        try
        {
            std::size_t consumed = 0;
            double value = std::stod(line, &consumed);
            if (consumed == line.size())
            {
                out = value;
                return true;
            }
        }
        catch (...)
        {
        }
        std::cout << red("  Please enter a number.\n");
    }
    return false;
}

/// Read a calendar date (YYYY-MM-DD), re-asking until it parses.
/// Serial day numbers never reach the user — the calendar layer
/// (Date, proleptic Gregorian) sits between the prompt and the
/// scheduler. Returns false on EOF.
static bool readDate(const std::string &prompt, int &serialOut)
{
    std::string line;
    while (readToken(prompt, line))
    {
        if (Date::parseISO(line, serialOut) && serialOut >= Date::MIN_SERIAL)
            return true;
        std::cout << red("  Invalid date - use YYYY-MM-DD (e.g. 2026-11-09).\n");
    }
    return false;
}

static int readMenuChoice(const std::string &prompt, int minChoice, int maxChoice)
{
    int choice;
    while (readInt(prompt, choice))
    {
        if (choice >= minChoice && choice <= maxChoice)
            return choice;
        std::cout << red("  Choice must be between ")
                  << minChoice << " and " << maxChoice << ".\n";
    }
    return maxChoice; // EOF -> last option (usually "exit/logout")
}

/* ============================================================
 *  Printing helpers
 * ============================================================ */

static void printBanner()
{
    const char *logo = R"(
  ____      _             _     _____                    _
 / ___|__ _| |_ ___ _ __ | |_  | ____|__  _ __ ___  ___| |_ ___ _ __
| |   / _` | __/ _ \ '_ \| __| |  _| \ \/ / '__/ _ \/ __| __/ _ \ '__|
| |__| (_| | ||  __/ | | | |_  | |___ >  <| | | (_) \__ \ ||  __/ |
 \____\__,_|\__\___|_| |_|\__| |_____/_/\_\_|  \___/|___/\__\___|_|)";
    std::cout << bold(cyan(logo)) << "\n";
    std::cout << yellow("        Console edition | C++17 | hand-built data structures\n\n");
}

static void printCarTableHeader()
{
    std::ostringstream oss;
    oss << "  " << std::left << std::setw(16) << "NAME" << std::setw(12)
        << "BRAND" << std::right << std::setw(10) << "PRICE/DAY"
        << "  " << std::left << std::setw(12) << "STATUS"
        << std::right << std::setw(6) << "QUEUE";
    std::cout << bold(oss.str()) << "\n";
    std::cout << "  " << std::string(62, '-') << "\n";
}

static void printCarRow(const Car *car)
{
    std::string status = toString(car->status());
    if (status == "AVAILABLE")
        status = green(status);
    else if (status == "MAINTENANCE")
        status = red(status);
    else if (status == "RENTED")
        status = yellow(status);

    std::ostringstream price;
    price << std::fixed << std::setprecision(1) << car->pricePerDay();

    std::cout << "  " << std::left << std::setw(16) << car->name()
              << std::setw(12) << car->brand()
              << std::right << std::setw(10) << price.str()
              << "  " << std::left << std::setw(12) << status
              << std::right << std::setw(4) << car->reservationCount()
              << std::setw(2) << " booking(s)"
              << "\n";
}

static void printFleet(RentalManager &system, bool sorted)
{
    std::size_t count = system.fleetSize();
    if (count == 0)
    {
        std::cout << yellow("\n  The fleet is empty. Ask the manager to add cars.\n");
        return;
    }

    std::cout << "\n" << bold("  FLEET (" + std::to_string(count) + " cars"
                               + (sorted ? ", sorted by name" : ", insertion order") + ")\n");
    printCarTableHeader();
    auto printRow = [](Car *car)
    { printCarRow(car); };
    if (sorted)
        system.forEachCarSorted(printRow);
    else
        system.forEachCar(printRow);
}

/* ============================================================
 *  Pickup reminders (v3.1.0)
 * ============================================================
 *  The customer-facing half of the reminder maths: everything due
 *  this week, soonest first, phrased the way the desk would say
 *  it. Purely read-only — an empty week prints nothing at all, so
 *  scripted flows and one-off screens stay untouched.
 * ============================================================ */
static void printPickupReminders(const RentalManager &system)
{
    const std::vector<PickupReminder> pickups = system.upcomingPickups(
        REMINDER_HORIZON_DAYS, Date::todaySerial());
    if (pickups.empty())
        return;

    std::cout << bold(cyan("\n===== PICKUP REMINDERS | next "
                           + std::to_string(REMINDER_HORIZON_DAYS)
                           + " days =====\n"));
    for (const PickupReminder &p : pickups)
    {
        std::cout << "  #" << p.reservationId
                  << " | " << analyticsPad(p.carName, 16)
                  << " | " << Date::toISO(p.start) << "  "
                  << yellow(reminderPhrase(p.daysAway));
        if (p.seriesId > 0)
            std::cout << yellow("  [series #"
                                + std::to_string(p.seriesId) + "]");
        std::cout << "\n";
    }
    std::cout << yellow("  Review or cancel them in My reservations.\n");
}

static void printReservations(const std::vector<Reservation *> &list)
{
    if (list.empty())
    {
        std::cout << yellow("\n  You have no reservations yet.\n");
        return;
    }
    std::cout << bold("\n  YOUR RESERVATIONS\n");
    std::cout << "  " << std::string(66, '-') << "\n";
    for (const Reservation *r : list)
    {
        std::cout << "  #" << r->id()
                  << " | " << std::left << std::setw(16) << r->car()->name()
                  << " | " << Date::toISO(r->startDate()) << " -> "
                  << Date::toISO(r->endDate())
                  << " (" << r->durationDays() << " day(s))";
        // (v3.0.0) Occurrences of a recurring series carry the shared
        // series tag, so the customer can cancel the rest in one step.
        if (r->seriesId() > 0)
            std::cout << yellow("  [series #"
                                + std::to_string(r->seriesId()) + "]");
        std::cout << "\n";
    }
}

/// The logged-in customer's waitlist wishes (v2.6.0).
static void printWaitlist(const std::vector<WaitlistEntry *> &list)
{
    if (list.empty())
    {
        std::cout << yellow("\n  You are not waiting for any car.\n");
        return;
    }
    std::cout << bold("\n  YOUR WAITLIST\n");
    std::cout << "  " << std::string(60, '-') << "\n";
    for (const WaitlistEntry *e : list)
        std::cout << "  wish #" << e->joinOrder()
                  << " | " << std::left << std::setw(16) << e->car()->name()
                  << " | " << Date::toISO(e->startDate()) << " -> "
                  << Date::toISO(e->endDate())
                  << " (" << e->durationDays() << " day(s))\n";
    std::cout << yellow("  We auto-book you the moment a wished range frees up.\n");
}

/* ============================================================
 *  Screens: authentication
 * ============================================================ */

static void registerScreen(RentalManager &system)
{
    std::cout << bold(cyan("\n--- Customer Registration ---\n"));
    std::string username, password;
    if (!readToken("  Username: ", username))
        return;
    if (!readToken("  Password (min 4 chars): ", password))
        return;

    if (system.registerCustomer(username, password))
        std::cout << green("  Account created. You can log in now.\n");
    else if (system.userExists(username))
        std::cout << red("  Username already taken.\n");
    else
        std::cout << red("  Password must be at least 4 characters.\n");
}

static void loginScreen(RentalManager &system)
{
    std::cout << bold(cyan("\n--- Login ---\n"));
    std::string username, password;
    if (!readToken("  Username: ", username))
        return;
    if (!readToken("  Password: ", password))
        return;

    if (system.login(username, password))
    {
        std::cout << green("  Welcome, " + username + " ("
                           + system.currentUserRole() + ")!\n");
        // (v3.1.0) A customer with pickups due this week hears
        // about them the moment they sign in. Non-customers hold
        // no pickups, so the screen stays quiet for staff.
        printPickupReminders(system);
    }
    else if (system.userExists(username) && system.isUserBlocked(username))
        std::cout << red("  This account is blocked. Contact the manager.\n");
    else if (system.userExists(username))
        std::cout << red("  Wrong password. 3 failed attempts block the account!\n");
    else
        std::cout << red("  No such user.\n");
}

/* ============================================================
 *  Screens: customer
 * ============================================================ */

/// Format money values with exactly 2 decimals.
static std::string money(double amount)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << amount;
    return oss.str();
}

/// (v3.0.0) The booking-time money preview: base, discount from
/// duration + the customer's CURRENT rank, and the estimated
/// total. The return invoice re-prices with the rank held that
/// day — the estimate says so, and late fees are never included.
static void printBookingEstimate(const Car *car, int start, int end,
                                 LoyaltyTier tier)
{
    const int days = end - start + 1;
    const PriceBreakdown q = quoteRental(days, car->pricePerDay(), tier);
    const std::string note = discountNote(q);
    std::cout << "  Estimate: " << days << " day(s) x "
              << money(car->pricePerDay()) << " = " << money(q.base);
    if (!note.empty())
        std::cout << yellow("  (" + note + ")");
    std::cout << "\n"
                 "  Pay on delivery about " << bold(money(q.total))
              << " — late fees are never discounted.\n";
}

/* ============================================================
 *  Loyalty card (customer option 10) and redeem flow (option 11)
 * ============================================================
 *  Pure renderers over the Loyalty.h rules: the progress bar to
 *  the next rank reuses the analytics bar primitives, so every
 *  number the customer sees is pinned by the unit tests.
 * ============================================================ */
static void printLoyaltyCard(const std::string &username,
                             const LoyaltyProfile &p)
{
    std::cout << bold(cyan("\n===== LOYALTY CARD | " + username + " =====\n"));
    std::cout << "\n  Rank: " << bold(toString(p.tier())) << "\n";

    if (p.tier() == LoyaltyTier::Platinum)
    {
        std::cout << "  You hold the top rank of the program.\n";
    }
    else
    {
        const LoyaltyTier next = nextTier(p.tier());
        const int span = tierThreshold(next) - tierThreshold(p.tier());
        const int done = p.lifetimePoints - tierThreshold(p.tier());
        std::cout << "   " << analyticsBar(done, span, 28) << "  "
                  << p.pointsToNextTier() << " points to " << toString(next)
                  << "\n";
    }

    std::ostringstream rate;
    rate << std::fixed << std::setprecision(2) << tierMultiplier(p.tier());
    std::cout << "  Points available  : " << p.points << "\n"
                 "  Lifetime points   : " << p.lifetimePoints << "\n"
                 "  Completed rentals : " << p.completedRentals << "\n"
                 "  Earning rate      : x" << rate.str()
              << " - 1 point per 10.00 spent, credited on return\n"
                 "  Redemption        : 10 points = 1.00 off your bill"
                 " (min 50, steps of 10)\n";
}

static void customerMenu(RentalManager &system, bool &running)
{
    std::cout << bold(cyan("\n===== CUSTOMER MENU | " + system.currentUsername()
                           + " | balance due: " + money(system.currentCustomerBalance())
                           + " =====\n"));
    std::cout << "  1. View fleet\n"
                 "  2. Search car by name\n"
                 "  3. My reservations\n"
                 "  4. Make a reservation\n"
                 "  5. Find cars free for a date range\n"
                 "  6. Join a waitlist\n"
                 "  7. My waitlist (leave)\n"
                 "  8. Renew a rental\n"
                 "  9. Pay my bill\n"
                 " 10. My loyalty card\n"
                 " 11. Redeem points on my bill\n"
                 " 12. Logout\n";

    switch (readMenuChoice("  Choice: ", 1, 12))
    {
    case 1:
        printFleet(system, false);
        break;
    case 2:
    {
        std::string name;
        if (!readToken("  Car name: ", name))
            return;
        Car *car = system.searchCar(name);
        if (car)
        {
            std::cout << bold("\n  FOUND:\n");
            printCarRow(car);
        }
        else
            std::cout << red("  No car named \"" + name + "\".\n");
        break;
    }
    case 3:
    {
        // (v3.1.0) The reminders ride above the list, so a customer
        // mid-session still sees what is due this week.
        printPickupReminders(system);

        const std::vector<Reservation *> mine = system.myReservations();
        printReservations(mine);

        // (v3.0.0) The recurring-series escape hatch: only offered
        // when the list actually contains a series, so scripted
        // one-off flows see no extra prompt.
        bool hasSeries = false;
        for (const Reservation *r : mine)
            if (r->seriesId() > 0)
                hasSeries = true;
        if (hasSeries)
        {
            std::string answer;
            if (!readLine(
                    "  Cancel a whole series? enter its #id (empty = stay): ",
                    answer) ||
                answer.empty())
                break;
            int seriesId = 0;
            try
            {
                seriesId = std::stoi(answer);
            }
            catch (...)
            {
                std::cout << red("  Not a series id.\n");
                break;
            }
            std::string error;
            const int removed = system.cancelRecurringSeries(seriesId, error);
            if (removed > 0)
                std::cout << green("  Series #" + std::to_string(seriesId)
                                   + ": " + std::to_string(removed)
                                   + " upcoming booking(s) cancelled.\n");
            else
                std::cout << red("  " + error + "\n");
        }
        break;
    }
    case 4:
    {
        std::string name, error;
        int start, end;
        if (!readToken("  Car name: ", name))
            return;
        if (!readDate("  Start date (YYYY-MM-DD): ", start))
            return;
        if (!readDate("  End date   (YYYY-MM-DD): ", end))
            return;

        // (v3.0.0) Optional recurrence: a series books the same car
        // and duration again one plan step later, as many times as
        // requested — atomically, or not at all.
        int repeat = 0;
        if (!readInt("  Repeat: 0 none, 1 weekly, 2 biweekly, 3 monthly: ",
                     repeat))
            return;

        if (repeat == 0)
        {
            if (system.makeReservation(name, start, end, error))
            {
                std::cout << green("  Reservation confirmed! Pay on delivery.\n");
                // (v3.0.0) Money preview with the CURRENT rank.
                if (Car *booked = system.searchCar(name))
                    printBookingEstimate(booked, start, end,
                                         system.myLoyalty().tier());
            }
            else
            {
                std::cout << red("  Reservation failed: " + error + "\n");
                if (error.find("overlap") != std::string::npos)
                    std::cout << yellow(
                        "  Tip: join the waitlist (option 6) and we book you\n"
                        "       automatically when the range frees up.\n");
            }
            break;
        }

        RecurrencePlan plan;
        if (!planFromChoice(repeat, plan))
        {
            std::cout << red("  Choice must be between 0 and 3.\n");
            break;
        }
        int times;
        if (!readInt("  Book it how many times (2-8): ", times))
            return;

        int seriesId = 0;
        if (system.makeRecurringReservation(name, start, end, plan, times,
                                            seriesId, error))
        {
            std::cout << green("  Series #" + std::to_string(seriesId)
                               + " booked: " + std::to_string(times) + " x "
                               + toString(plan) + " on " + name + "\n");
            const int durationDays = end - start + 1;
            for (int i = 0; i < times; ++i)
            {
                const int s = occurrenceStart(plan, start, i);
                std::cout << "    pickup " << (i + 1) << ": "
                          << Date::toISO(s) << " -> "
                          << Date::toISO(s + durationDays - 1) << "\n";
            }
            // (v3.0.0) Per-occurrence money preview + series total.
            if (Car *booked = system.searchCar(name))
            {
                const PriceBreakdown q = quoteRental(
                    durationDays, booked->pricePerDay(),
                    system.myLoyalty().tier());
                std::cout << "  Estimate per pickup: " << durationDays
                          << " day(s) x " << money(booked->pricePerDay())
                          << " = " << money(q.base);
                if (q.discount > 0.0)
                    std::cout << yellow(" -> " + money(q.total) + " after "
                                        + discountNote(q).substr(
                                            discountNote(q).find('(')));
                std::cout << "\n"
                             "  Series total about "
                          << bold(money(q.total * times))
                          << " (late fees are never discounted).\n";
            }
            std::cout << yellow(
                "  The whole series is cancelled at once from\n"
                "       My reservations (option 3) — just give its #id.\n");
        }
        else
            std::cout << red("  Series booking failed: " + error + "\n");
        break;
    }
    case 5:
    {
        int start, end;
        if (!readDate("  Start date (YYYY-MM-DD): ", start))
            return;
        if (!readDate("  End date   (YYYY-MM-DD): ", end))
            return;
        std::vector<Car *> freeCars = system.findAvailableCars(start, end);
        if (freeCars.empty())
        {
            std::cout << red("  No car is free for " + Date::toISO(start)
                             + " -> " + Date::toISO(end) + ".\n");
            std::cout << yellow("  Tip: join a car's waitlist (option 6) to be\n"
                                "       auto-booked when a window opens up.\n");
            break;
        }
        std::cout << bold("\n  FREE FOR " + Date::toISO(start) + " -> "
                          + Date::toISO(end) + " ("
                          + std::to_string(freeCars.size()) + " car(s))\n");
        printCarTableHeader();
        for (const Car *car : freeCars)
            printCarRow(car);
        break;
    }
    case 6:
    {
        std::string name, error;
        int start, end;
        if (!readToken("  Car name: ", name))
            return;
        if (!readDate("  Desired start (YYYY-MM-DD): ", start))
            return;
        if (!readDate("  Desired end   (YYYY-MM-DD): ", end))
            return;
        if (system.joinWaitlist(name, start, end, error))
            std::cout << green("  You are on the waitlist for \"" + name
                               + "\". We book you automatically when it frees up.\n");
        else
            std::cout << red("  Waitlist failed: " + error + "\n");
        break;
    }
    case 7:
    {
        printWaitlist(system.myWaitlist());
        std::string name;
        if (!readLine("  Car to leave (empty = stay): ", name) || name.empty())
            break;
        std::string error;
        if (system.leaveWaitlist(name, error))
            std::cout << green("  You left the waitlist of \"" + name + "\".\n");
        else
            std::cout << red("  " + error + "\n");
        break;
    }
    case 8:
    {
        std::string name, error;
        int days;
        if (!readToken("  Car name: ", name))
            return;
        if (!readInt("  Extra days: ", days))
            return;
        if (system.renewRental(name, days, error))
            std::cout << green("  Rental extended by " + std::to_string(days)
                               + " day(s). Extra charges apply at return.\n");
        else
            std::cout << red("  Renewal failed: " + error + "\n");
        break;
    }
    case 9:
    {
        double owed = system.currentCustomerBalance();
        if (owed <= 0.0)
        {
            std::cout << green("  Your balance is clear. Nothing to pay.\n");
        }
        else
        {
            double paid = system.payBill();
            std::cout << green("  Receipt: paid " + money(paid) + ". Thank you!\n");
        }
        break;
    }
    case 10:
        printLoyaltyCard(system.currentUsername(), system.myLoyalty());
        break;
    case 11:
    {
        const LoyaltyProfile profile = system.myLoyalty();
        const double owed = system.currentCustomerBalance();
        std::cout << bold(cyan("\n--- REDEEM LOYALTY POINTS ---\n"));
        std::cout << "  Points available: " << profile.points
                  << "  |  bill due: " << money(owed) << "\n";
        if (owed <= 0.0)
        {
            std::cout << yellow(
                "  Your bill is clear - points are redeemed against a bill,\n"
                "       so come back after your next rental.\n");
            break;
        }
        int points;
        if (!readInt("  Points to redeem (min 50, steps of 10): ", points))
            return;
        std::string error;
        const double credited = system.redeemPoints(points, error);
        if (credited >= 0.0)
            std::cout << green("  Credit of " + money(credited)
                               + " applied. Bill now: "
                               + money(system.currentCustomerBalance())
                               + ", points left: "
                               + std::to_string(system.myLoyalty().points)
                               + ".\n");
        else
            std::cout << red("  Redemption failed: " + error + "\n");
        break;
    }
    case 12:
        system.logout();
        std::cout << yellow("  Logged out.\n");
        break;
    default:
        break;
    }
    (void)running;
}

/* ============================================================
 *  Screens: staff
 * ============================================================ */

static void staffMenu(RentalManager &system, bool &running)
{
    std::cout << bold(cyan("\n===== STAFF MENU | " + system.currentUsername() + " =====\n"));
    std::cout << "  1. View fleet\n"
                 "  2. Search car by name\n"
                 "  3. Deliver car to customer\n"
                 "  4. Process car return (with penalty)\n"
                 "  5. Send car to maintenance\n"
                 "  6. Finish maintenance\n"
                 "  7. Show maintenance history\n"
                 "  8. Serve waitlist now\n"
                 "  9. Logout\n";

    switch (readMenuChoice("  Choice: ", 1, 9))
    {
    case 1:
        printFleet(system, false);
        break;
    case 2:
    {
        std::string name;
        if (!readToken("  Car name: ", name))
            return;
        Car *car = system.searchCar(name);
        if (car)
        {
            std::cout << bold("\n  FOUND:\n");
            printCarRow(car);
        }
        else
            std::cout << red("  No car named \"" + name + "\".\n");
        break;
    }
    case 3:
    {
        std::string name, error;
        if (!readToken("  Car name: ", name))
            return;
        if (system.deliverCar(name, error))
            std::cout << green("  Car handed over. Drive safe!\n");
        else
            std::cout << red("  Delivery failed: " + error + "\n");
        break;
    }
    case 4:
    {
        std::string name, error;
        int day;
        if (!readToken("  Car name: ", name))
            return;
        if (!readDate("  Actual return date (YYYY-MM-DD): ", day))
            return;
        if (system.returnCar(name, day, error))
        {
            std::cout << green("  Return processed and invoiced.\n");
            // (v3.0.0) The discount the return earned, if any.
            const std::string discount = system.lastDiscountNote();
            if (!discount.empty())
                std::cout << green("  Discount applied: " + discount + "\n");
            // (v2.9.0) A threshold crossed on this return is worth
            // announcing at the desk.
            const std::string rankUp = system.lastLoyaltyRankUp();
            if (!rankUp.empty())
                std::cout << yellow(
                    "  Rank up! " + rankUp
                    + " - the new multiplier applies from the next rental.\n");
        }
        else
            std::cout << red("  Return failed: " + error + "\n");
        break;
    }
    case 5:
    {
        std::string name, desc, error;
        int maintDay;
        double cost;
        if (!readToken("  Car name: ", name))
            return;
        if (!readDate("  Date (YYYY-MM-DD): ", maintDay))
            return;
        if (!readLine("  Description: ", desc) || desc.empty())
            desc = "general service";
        if (!readDouble("  Cost: ", cost))
            return;
        if (system.recordMaintenance(name, Date::toISO(maintDay), desc, cost, error))
            std::cout << yellow("  Car moved to MAINTENANCE. Log saved.\n");
        else
            std::cout << red("  Failed: " + error + "\n");
        break;
    }
    case 6:
    {
        std::string name;
        if (!readToken("  Car name: ", name))
            return;
        if (system.finishMaintenance(name))
            std::cout << green("  Maintenance finished; car is back in service.\n");
        else
            std::cout << red("  Failed: car not found or not in maintenance.\n");
        break;
    }
    case 7:
    {
        std::string name;
        if (!readToken("  Car name: ", name))
            return;
        Car *car = system.searchCar(name);
        if (!car)
        {
            std::cout << red("  No car named \"" + name + "\".\n");
            break;
        }
        const auto &log = car->maintenanceLog();
        if (log.empty())
        {
            std::cout << green("  No maintenance recorded for this car.\n");
            break;
        }
        std::cout << bold("\n  MAINTENANCE LOG | " + car->name() + "\n");
        for (const auto &rec : log)
            std::cout << "  " << rec.date << " | " << rec.description
                      << " | cost " << rec.cost << "\n";
        break;
    }
    case 8:
    {
        std::string name, error;
        if (!readToken("  Car name: ", name))
            return;
        if (system.serveWaitlist(name, error))
        {
            Car *car = system.searchCar(name);
            const Reservation *r = car ? car->currentReservation() : nullptr;
            std::string who = r ? r->customer()->username() : "?";
            std::cout << green("  Booked " + who + " from the waitlist.\n");
        }
        else
            std::cout << red("  Serve failed: " + error + "\n");
        break;
    }
    case 9:
        system.logout();
        std::cout << yellow("  Logged out.\n");
        break;
    default:
        break;
    }
    (void)running;
}

/* ============================================================
 *  Screens: manager
 * ============================================================ */

/// Pretty-print the aggregate snapshot computed by the manager.
static void printFleetStats(const FleetStats &s)
{
    std::cout << bold("\n  FLEET STATISTICS\n");
    std::cout << "  " << std::string(50, '-') << "\n";

    auto row = [](const std::string &label, const std::string &value)
    {
        std::cout << "  " << std::left << std::setw(24) << label
                  << std::right << std::setw(26) << value << "\n";
    };

    row("Fleet size", std::to_string(s.totalCars) + " car(s)");
    row("Available", std::to_string(s.availableCars));
    row("Reserved", std::to_string(s.reservedCars));
    row("Rented", std::to_string(s.rentedCars));
    row("Under maintenance", std::to_string(s.maintenanceCars));
    row("Active bookings", std::to_string(s.activeBookings));
    std::ostringstream util;
    util << std::fixed << std::setprecision(1) << s.utilization << "%";
    row("Fleet utilization", util.str());
    row("Gross income", money(s.grossIncome));
    row("Maintenance spend", money(s.maintenanceSpend));
    row("NET revenue", money(s.netRevenue));
    row("Top earner", s.topEarner.empty() ? "-" : s.topEarner);
}

/* ============================================================
 *  Fleet analytics dashboard (manager option 6)
 * ============================================================
 *  Renders the AnalyticsReport as labelled ASCII bar charts.
 *  All the math lives in the Analytics module — this function
 *  only draws, and it never mutates state.
 * ============================================================ */
static void renderDashboard(const RentalManager &system)
{
    const int today = Date::todaySerial();
    const AnalyticsReport report = buildAnalytics(system, today);

    std::cout << bold(cyan("\n===== FLEET ANALYTICS | " + Date::toISO(today)
                           + " =====\n"));

    const auto drawBars = [](const std::string &title,
                             const std::vector<NamedCount> &slices,
                             std::size_t labelWidth)
    {
        std::cout << "\n  " << bold(title) << "\n";
        int maxCount = 0;
        for (const NamedCount &slice : slices)
            maxCount = std::max(maxCount, slice.count);
        for (const NamedCount &slice : slices)
            std::cout << "   " << analyticsPad(slice.label, labelWidth) << "  "
                      << analyticsBar(slice.count, maxCount) << " "
                      << slice.count << "\n";
    };

    drawBars("Fleet composition", report.fleetByStatus, 13);
    drawBars("Fleet by daily-rate bucket", report.priceBuckets, 8);

    std::cout << "\n  " << bold("Active bookings per car") << "\n";
    if (report.bookingsPerCar.empty())
    {
        std::cout << "   (no live bookings)\n";
    }
    else
    {
        const int maxBookings = report.bookingsPerCar.front().count;
        for (const NamedCount &row : report.bookingsPerCar)
            std::cout << "   " << analyticsPad(row.label, 16) << "  "
                      << analyticsBar(row.count, maxBookings, 16) << " "
                      << row.count << "\n";
    }

    std::cout << "\n  " << bold("Waitlist depth per car") << "\n";
    if (report.waitlistPerCar.empty())
    {
        std::cout << "   (nobody is waiting)\n";
    }
    else
    {
        const int maxWaiting = report.waitlistPerCar.front().count;
        for (const NamedCount &row : report.waitlistPerCar)
            std::cout << "   " << analyticsPad(row.label, 16) << "  "
                      << analyticsBar(row.count, maxWaiting, 16) << " "
                      << row.count << "\n";
    }

    std::cout << "\n  " << bold("Upcoming pickups") << "\n";
    if (report.upcoming.empty())
    {
        std::cout << "   (nothing scheduled)\n";
    }
    else
    {
        for (const UpcomingPickup &p : report.upcoming)
        {
            std::string when;
            if (p.daysAway == 0)
                when = "today";
            else if (p.daysAway == 1)
                when = "tomorrow";
            else
                when = "in " + std::to_string(p.daysAway) + " days";
            std::cout << "   " << p.startDate << "  "
                      << analyticsPad(p.carName, 16) << "  "
                      << p.customer << "  (" << when << ")\n";
        }
    }

    if (report.overdueReturns > 0)
        std::cout << yellow(
            "\n  ! " + std::to_string(report.overdueReturns)
            + " booking(s) past their end date — chase up the returns.\n");
    if (report.totalWaitlisted > 0)
        std::cout << yellow(
            "  ! " + std::to_string(report.totalWaitlisted)
            + " customer(s) on a waitlist — serving one after every return.\n");
    std::cout << "\n  " << report.totalActiveBookings
              << " live booking(s) fleet-wide.\n";
}

/* ============================================================
 *  Audit trail (manager option 7)
 * ============================================================
 *  The facade records every privileged action, login and denial
 *  on its own; this screen only reads the newest entries back.
 * ============================================================ */
static void printAuditLog(const RentalManager &system)
{
    const std::vector<AuditEntry> tail = system.auditLog().recent(15);
    if (tail.empty())
    {
        std::cout << yellow("  The audit trail is empty.\n");
        return;
    }

    const auto pad = [](const std::string &text, std::size_t width)
    {
        std::string out = text;
        if (out.size() > width)
            out = out.substr(0, width);
        while (out.size() < width)
            out += ' ';
        return out;
    };

    std::cout << bold(cyan("\n  AUDIT TRAIL — newest "
                           + std::to_string(tail.size()) + " of "
                           + std::to_string(system.auditLog().size())
                           + " entry(ies)\n"));
    std::cout << "  DATE       ACTOR        ACTION"
                 "               RESULT  DETAIL\n"
                 "  ---------------------------------------------------------"
                 "------------\n";
    for (const AuditEntry &e : tail)
    {
        const std::string line =
            "  " + pad(e.date, 10) + " " + pad(e.actor, 12) + " "
            + pad(e.action, 20) + "  "
            + (e.allowed ? std::string("ok     ") : std::string("DENIED "))
            + e.detail.substr(0, 30) + "\n";
        if (e.allowed)
            std::cout << line;
        else
            std::cout << red(line);
    }
}

/* ============================================================
 *  Loyalty program overview (manager option 8, v2.7.0)
 * ============================================================
 *  Renders the facade's LoyaltyOverview: members per rank as
 *  bars, the outstanding-points liability and the top member.
 *  Reads state only - never mutates.
 * ============================================================ */
static void renderLoyaltyOverview(const RentalManager &system)
{
    const LoyaltyOverview o = system.loyaltyOverview();
    std::cout << bold(cyan("\n===== LOYALTY PROGRAM OVERVIEW =====\n"));
    if (o.totalMembers == 0)
    {
        std::cout << yellow("\n  No customers are enrolled yet.\n");
        return;
    }

    const char *tierNames[4] = {"Bronze", "Silver", "Gold", "Platinum"};
    int maxMembers = 0;
    for (const int count : o.membersByTier)
        maxMembers = std::max(maxMembers, count);
    std::cout << "\n  Members per rank\n";
    for (int i = 0; i < 4; ++i)
        std::cout << "   " << analyticsPad(tierNames[i], 9) << "  "
                  << analyticsBar(o.membersByTier[i], maxMembers, 20) << " "
                  << o.membersByTier[i] << "\n";

    std::cout << "\n  Total members      : " << o.totalMembers << "\n"
                 "  Outstanding points : " << o.outstandingPoints << "\n"
                 "  Potential credits  : " << money(o.pointsLiability) << "\n"
                 "  Top member         : ";
    if (o.topMember.empty())
        std::cout << "-\n";
    else
        std::cout << o.topMember << " (" << o.topMemberLifetime
                  << " lifetime points)\n";

    // (v2.9.0) Recent promotions, straight from the audit trail —
    // the five newest loyalty.rankup entries, still chronological.
    std::vector<AuditEntry> rankUps;
    for (const AuditEntry &e : system.auditLog().all())
        if (e.action == "loyalty.rankup")
            rankUps.push_back(e);
    if (rankUps.size() > std::size_t(5))
        rankUps.erase(rankUps.begin(), rankUps.end() - 5);
    std::cout << "\n  Recent rank-ups\n";
    if (rankUps.empty())
        std::cout << "   none yet\n";
    else
        for (const AuditEntry &e : rankUps)
            std::cout << "   " << e.date << "  " << e.detail << "\n";
}

/* ============================================================
 *  Monthly report (manager option 17, v3.0.0)
 * ============================================================
 *  Renders the MonthlyReport as the same labelled ASCII bars the
 *  analytics dashboard uses. All the aggregation lives in
 *  MonthlyReport.cpp — this function only draws, and it never
 *  mutates state.
 * ============================================================ */
static void renderMonthlyReport(const MonthlyReport &r)
{
    std::cout << bold(cyan("\n===== MONTHLY REPORT | " + r.month + " =====\n"));
    if (r.rentalsCompleted == 0)
    {
        std::cout << yellow("\n  No rentals were completed in " + r.month
                            + ".\n");
        return;
    }

    std::cout << "\n  Rentals completed : " << r.rentalsCompleted << "\n"
                 "  Revenue booked    : " << money(r.revenue)
              << "   (discounts given: " << money(r.discountGiven)
              << " | late fees: " << money(r.lateFees) << ")\n"
                 "  Distinct customers: " << r.distinctCustomers << "\n";

    int maxCarRentals = 0;
    for (const CarRevenue &car : r.perCar)
        maxCarRentals = std::max(maxCarRentals, car.rentals);
    std::cout << "\n  " << bold("Revenue by car") << "\n";
    for (const CarRevenue &car : r.perCar)
        std::cout << "   " << analyticsPad(car.name, 16) << "  "
                  << analyticsBar(car.rentals, maxCarRentals, 16) << " "
                  << car.rentals << " rental(s), " << money(car.revenue)
                  << "\n";

    int maxDayReturns = 0;
    for (const NamedCount &day : r.perDay)
        maxDayReturns = std::max(maxDayReturns, day.count);
    std::cout << "\n  " << bold("Returns per day") << "\n";
    for (const NamedCount &day : r.perDay)
        std::cout << "   " << analyticsPad(day.label, 4) << "  "
                  << analyticsBar(day.count, maxDayReturns, 16) << " "
                  << day.count << "\n";

    char day[4];
    std::snprintf(day, sizeof day, "%02d", r.topDay);
    std::cout << "\n  Busiest day: " << day << " ("
              << r.topDayRentals << " return(s))\n";
}

static void managerMenu(RentalManager &system, bool &running)
{
    std::cout << bold(cyan("\n===== MANAGER MENU | " + system.currentUsername() + " =====\n"));
    std::cout << "  1. Add car\n"
                 "  2. View fleet (insertion order)\n"
                 "  3. View fleet (sorted by name - AVL index)\n"
                 "  4. Search fleet by keyword\n"
                 "  5. Fleet statistics\n"
                 "  6. Fleet analytics dashboard\n"
                 "  7. Audit log (who did what)\n"
                 "  8. Loyalty program overview\n"
                 "  9. Income report\n"
                 " 10. Export CSV & JSON reports\n"
                 " 11. Block user\n"
                 " 12. Unblock user\n"
                 " 13. Create staff account\n"
                 " 14. Save data to disk\n"
                 " 15. Save & logout\n"
                 " 16. Exit program\n"
                 " 17. Monthly report (revenue & bookings)\n";

    switch (readMenuChoice("  Choice: ", 1, 17))
    {
    case 1:
    {
        std::string name, brand;
        double price;
        if (!readToken("  Car name: ", name))
            return;
        if (!readToken("  Brand: ", brand))
            return;
        if (!readDouble("  Price per day: ", price))
            return;
        if (system.addCar(new Car(name, brand, price)))
            std::cout << green("  Car added to the fleet.\n");
        else
            std::cout << red("  A car with this name already exists.\n");
        break;
    }
    case 2:
        printFleet(system, false);
        break;
    case 3:
        printFleet(system, true);
        break;
    case 4:
    {
        std::string keyword;
        if (!readToken("  Keyword (name or brand): ", keyword))
            return;
        std::vector<Car *> matches = system.searchFleet(keyword);
        if (matches.empty())
        {
            std::cout << red("  Nothing matches \"" + keyword + "\".\n");
            break;
        }
        std::cout << bold("\n  MATCHES (" + std::to_string(matches.size())
                          + ")\n");
        printCarTableHeader();
        for (const Car *car : matches)
            printCarRow(car);
        break;
    }
    case 5:
        printFleetStats(system.computeStats());
        break;
    case 6:
        renderDashboard(system);
        break;
    case 7:
        printAuditLog(system);
        break;
    case 8:
        renderLoyaltyOverview(system);
        break;
    case 9:
        system.printIncomeReport();
        break;
    case 10:
    {
        const bool fleetOk = system.exportFleetReport("reports/fleet_report.csv");
        const bool bookingsOk =
            system.exportBookingReport("reports/bookings_report.csv");
        const bool loyaltyOk =
            system.exportLoyaltyReport("reports/loyalty_report.csv");
        const bool jsonOk = exportJsonReport(system, "reports/report.json");
        if (fleetOk && bookingsOk && loyaltyOk && jsonOk)
            std::cout << green("  Reports written:\n"
                               "    reports/fleet_report.csv\n"
                               "    reports/bookings_report.csv\n"
                               "    reports/loyalty_report.csv\n"
                               "    reports/report.json\n");
        else
            std::cout << red("  Could not write the report files.\n");
        break;
    }
    case 11:
    {
        std::string user;
        if (!readToken("  Username to block: ", user))
            return;
        if (system.blockUser(user))
            std::cout << yellow("  User \"" + user + "\" blocked.\n");
        else
            std::cout << red("  No such user.\n");
        break;
    }
    case 12:
    {
        std::string user;
        if (!readToken("  Username to unblock: ", user))
            return;
        if (system.unblockUser(user))
            std::cout << green("  User \"" + user + "\" unblocked.\n");
        else
            std::cout << red("  No such user.\n");
        break;
    }
    case 13:
    {
        std::string user, pass, role;
        if (!readToken("  New username: ", user))
            return;
        if (!readToken("  Password (min 4 chars): ", pass))
            return;
        if (!readToken("  Role (Staff / Manager): ", role))
            return;
        if (system.registerStaffAccount(user, pass, role))
            std::cout << green("  " + role + " account created.\n");
        else
            std::cout << red("  Failed: check username is new, password >= 4 chars, "
                             "role is Staff or Manager.\n");
        break;
    }
    case 14:
        std::cout << (system.saveData() ? green("  Data saved to ./data\n")
                                        : red("  Could not write data files.\n"));
        break;
    case 15:
        system.saveData();
        system.logout();
        std::cout << yellow("  Data saved. Logged out.\n");
        break;
    case 16:
        system.saveData();
        std::cout << yellow("  Data saved. Goodbye!\n");
        running = false;
        break;
    case 17:
    {
        // (v3.0.0) One month at a glance, then the same numbers as
        // a CSV next to the other reports.
        int year;
        if (!readInt("  Year (e.g. 2026): ", year))
            return;
        const int month = readMenuChoice("  Month (1-12): ", 1, 12);

        const MonthlyReport r = system.monthlyReport(year, month);
        renderMonthlyReport(r);
        if (r.rentalsCompleted > 0)
        {
            std::ostringstream name;
            name << "reports/monthly_" << r.month << ".csv";
            std::string error;
            if (system.exportMonthlyReport(year, month, name.str(), error))
                std::cout << green("  Saved to " + name.str() + "\n");
            else
                std::cout << red("  Could not write " + name.str()
                                 + ": " + error + "\n");
        }
        break;
    }
    default:
        break;
    }
}

/* ============================================================
 *  main
 * ============================================================ */

int main(int argc, char *argv[])
{
    // Non-interactive queries exit before touching data/.
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--version" || arg == "-v")
        {
            std::cout << "Car Rental Management System v" << APP_VERSION
                      << " — C++17, zero dependencies\n";
            return 0;
        }
        if (arg == "--help" || arg == "-h")
        {
            std::cout << "Car Rental Management System v" << APP_VERSION << "\n"
                         "\n"
                         "Usage: car_rental [option]\n"
                         "\n"
                         "  (no option)    start the interactive console app\n"
                         "  -v, --version  print version and exit\n"
                         "  -h, --help     show this help and exit\n"
                         "\n"
                         "Demo accounts (auto-seeded on first run):\n"
                         "  manager  admin / manager123\n"
                         "  staff    sara  / staff123\n"
                         "  customer ali   / customer123\n";
            return 0;
        }
        std::cerr << "car_rental: unknown option '" << arg << "' — try --help\n";
        return 1;
    }

    RentalManager system;
    bool running = true;

    printBanner();

    // Load persisted state; a fresh clone gets a ready-to-demo fleet.
    if (system.loadData())
        std::cout << green("  Loaded saved state from ./data\n");
    else
    {
        system.seedDemoData();
        std::cout << yellow("  First run: demo fleet + accounts seeded.\n");
        std::cout << "    manager  -> admin / manager123\n"
                     "    staff    -> sara / staff123\n"
                     "    customer -> ali  / customer123\n";
    }

    while (running)
    {
        if (!system.isLoggedIn())
        {
            std::cout << bold(cyan("\n===== MAIN MENU =====\n"));
            std::cout << "  1. Register (customer)\n"
                         "  2. Login\n"
                         "  3. Exit\n";

            switch (readMenuChoice("  Choice: ", 1, 3))
            {
            case 1:
                registerScreen(system);
                break;
            case 2:
                loginScreen(system);
                break;
            case 3:
                running = false;
                std::cout << cyan("  Goodbye!\n");
                break;
            default:
                break;
            }
        }
        else
        {
            const std::string role = system.currentUserRole();
            if (role == "Customer")
                customerMenu(system, running);
            else if (role == "Staff")
                staffMenu(system, running);
            else
                managerMenu(system, running);
        }
    }

    // Persist everything so the next run continues where we left off.
    system.saveData();
    return 0;
}
