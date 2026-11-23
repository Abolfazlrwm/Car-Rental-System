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
#include <cstdlib> // NO_COLOR environment variable
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <io.h>
#define NOMINMAX // windows.h min/max macros break std::max/std::min below
#include <windows.h>
#define IS_TTY _isatty(_fileno(stdout))
#else
#include <unistd.h>
#define IS_TTY isatty(STDOUT_FILENO)
#endif

#include "manager_system/Analytics.h"
#include "manager_system/Cli.h"
#include "manager_system/Date.h"
#include "manager_system/I18n.h"
#include "manager_system/JsonExport.h"
#include "manager_system/Loyalty.h"
#include "manager_system/MonthlyReport.h"
#include "manager_system/Pdf.h"
#include "manager_system/Png.h"
#include "manager_system/Pricing.h"
#include "manager_system/Recurring.h"
#include "manager_system/RentalManager.h"
#include "manager_system/RestServer.h"

/* Program version, reported by --version / --help. */
static const char *APP_VERSION = "3.10.0";

/* ============================================================
 *  Terminal colors (no-ops when stdout is piped)
 * ============================================================ */

/* Colors start on for terminals (and the browser terminal); --no-color
 * or a NO_COLOR environment variable (no-color.org) turns them off. */
static bool g_colors = IS_TTY;

static std::string colorize(const std::string &text, const char *code)
{
    return g_colors ? std::string(code) + text + "\033[0m" : text;
}

static std::string bold(const std::string &s)
{
    return colorize(s, "\033[1m");
}
static std::string green(const std::string &s)
{
    return colorize(s, "\033[32m");
}
static std::string red(const std::string &s)
{
    return colorize(s, "\033[31m");
}
static std::string yellow(const std::string &s)
{
    return colorize(s, "\033[33m");
}
static std::string cyan(const std::string &s)
{
    return colorize(s, "\033[36m");
}

/* ============================================================
 *  Guided demo (v3.4.0) — `--demo` plays one full shift at the
 *  rental desk by feeding the very same readLine() a scripted
 *  sequence of keystrokes, with a narration banner before each
 *  beat and small pauses: book -> change a booking -> deliver ->
 *  late return with penalty -> manager reports.
 *
 *  Design notes:
 *    - one implementation serves desktop and browser: the pause
 *      is a thread sleep on desktop and an Asyncify sleep in the
 *      wasm build, and the script flows through the same
 *      readLine() a human types into;
 *    - it runs on a FRESH in-memory fleet — nothing is loaded
 *      from ./data and nothing is written back, so a presenter
 *      can run it anywhere without side effects;
 *    - if the script ever runs dry (menu drift), readLine falls
 *      through to normal interactive input — the demo hands the
 *      keyboard back instead of hanging or crashing.
 * ============================================================ */
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <chrono>
#include <thread>
#endif
#include <cstddef>
#include <vector>

struct DemoStep
{
    std::string narration; /// banner printed BEFORE this input ("" = none)
    std::string line;      /// line fed to readLine (as if typed)
};

static bool g_demoOn = false; /// --demo was requested
static std::vector<DemoStep> g_demoScript;
static std::size_t g_demoCursor = 0;

/// Portable pause: Asyncify sleep in the browser, thread sleep on desktop.
static void demoPause(int ms)
{
#ifdef __EMSCRIPTEN__
    emscripten_sleep(ms);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#endif
}

/// Narration between beats: multi-line blocks print as-is, single
/// lines get the "DEMO ·" banner.
static void demoSay(const std::string &scene)
{
    if (scene.find('\n') != std::string::npos)
        std::cout << cyan(scene) << "\n";
    else
        std::cout << "\n" << cyan("  ===== DEMO · " + scene + " =====") << "\n";
    std::cout.flush();
    demoPause(1100);
}

/// Pop the next scripted line, narrating and echoing it like a real
/// keystroke. False when not in demo mode or the script is over.
static bool demoNext(const std::string &prompt, std::string &out)
{
    if (!g_demoOn || g_demoCursor >= g_demoScript.size())
        return false;
    const DemoStep &st = g_demoScript[g_demoCursor++];
    if (!st.narration.empty())
        demoSay(st.narration);
    std::cout << prompt;
    std::cout.flush();
    demoPause(420);
    std::cout << yellow(st.line) << "\n"; // echo the scripted keystrokes
    std::cout.flush();
    demoPause(280);
    out = st.line;
    while (!out.empty() && out.back() == ' ')
        out.pop_back();
    return true;
}

/// Build the scripted shift. Dates derive from today's clock so the
/// booking is always fresh: pickup = today+3, hand-back = today+6,
/// moved pickup = today+4, processed two days late = today+8 (the
/// monthly report then reads that month).
static void demoBuildScript()
{
    const int today = Date::todaySerial();
    const std::string start = Date::toISO(today + 3);
    const std::string end = Date::toISO(today + 6);
    const std::string movedStart = Date::toISO(today + 4);
    const std::string movedEnd = Date::toISO(today + 7);
    const std::string processed = Date::toISO(today + 8); // 2 days late
    const std::string repYear = processed.substr(0, 4);
    const std::string repMonth = processed.substr(5, 2);

    const std::string intro =
        "\n"
        "  ==================================================================\n"
        "   GUIDED DEMO — one full shift at the rental desk, zero typing:\n"
        "   book -> change a booking -> deliver -> late return with penalty\n"
        "   -> manager reports.  Fresh in-memory fleet: ./data is never read\n"
        "   or written; the monthly report's CSV export still runs as usual.\n"
        "   Yellow lines are the scripted keystrokes; sit back and watch.\n"
        "  ==================================================================\n";
    const std::string outro =
        "\n"
        "  ==================================================================\n"
        "   THAT'S ONE FULL SHIFT — every number above came out of the same\n"
        "   hand-built AVL / hash table / heap / linked-list core.\n"
        "   Your turn — start the interactive app and log in:\n"
        "       manager  admin / manager123\n"
        "       staff    sara  / staff123\n"
        "       customer ali   / customer123\n"
        "  ==================================================================\n";

    const DemoStep steps[] = {
        {intro, "2"},
        {"THE CUSTOMER — Ali checks in", "ali"},
        {"", "customer123"},
        {"PICKUP REMINDERS — the week ahead, as the desk would say it", "3"},
        {"A NEW BOOKING — the Benz C200, pickup three days out", "4"},
        {"", "Benz C200"},
        {"", start},
        {"", end},
        {"THE ESTIMATE — long-rental x rank discount stack, pay on delivery", "0"},
        {"CHANGE A BOOKING — option 13 moves ONE booking, the schedule is "
         "re-checked",
         "13"},
        {"", "1"},
        {"", "1"},
        {"", movedStart},
        {"RE-QUOTED — new dates, new price, everything else untouched", movedEnd},
        {"LOYALTY CARD — rank, progress bar, earning rate", "10"},
        {"", "12"},
        {"THE STAFF — Sara takes over the desk", "2"},
        {"", "sara"},
        {"", "staff123"},
        {"HANDOVER — keys out the door", "3"},
        {"", "Benz C200"},
        {"THE RETURN — two days late: the penalty lands on the invoice", "4"},
        {"", "Benz C200"},
        {"", processed},
        {"", "9"},
        {"THE MANAGER — admin opens the books", "2"},
        {"", "admin"},
        {"", "manager123"},
        {"INCOME REPORT — gross, maintenance, net per car", "9"},
        {"MONTHLY REPORT — revenue, per-car bars, busiest day", "17"},
        {"", repYear},
        {"", repMonth},
        {outro, "15"},
        {"", "3"},
    };
    g_demoScript.assign(std::begin(steps), std::end(steps));
}

/* ============================================================
 *  Input helpers — the anti-segfault, anti-infinite-loop layer
 * ============================================================ */

#ifdef __EMSCRIPTEN__
/* The browser demo (make wasm): the page queues keystrokes through
 * Module.pushInput, and readLine waits for a FULL line here — polling
 * with emscripten_sleep (Asyncify) between checks — then copies the
 * bytes in one go. Sleeping happens in plain wasm (never inside a
 * JS import), so the UI never spins, the program never sees EOF and
 * can only exit through its own Exit menu. */
#include <cstdint>
#include <emscripten.h>

EM_JS(int, web_line_pending, (), { return __crsLineReady() ? 1 : 0; });

EM_JS(int, web_line_take, (int buf, int maxlen), { return __crsTakeLine(buf, maxlen); });

/// (v3.10.0) Hand a saved report to the demo page: the page copies
/// the bytes out of the WASM heap and shows a one-click download.
EM_JS(void, crs_offer_file, (const char *name, const unsigned char *bytes, int len), {
    if (Module.crsFileOffered)
        Module.crsFileOffered(UTF8ToString(name),
                              new Uint8Array(Module.HEAPU8.subarray(bytes, bytes + len)));
});

/// Read a just-written report back from the in-memory file system and
/// offer it to the demo page under its own file name.
static void offerReportDownload(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return;
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)),
                                     std::istreambuf_iterator<char>());
    if (bytes.empty())
        return;
    const std::size_t slash = path.find_last_of("/\\");
    crs_offer_file(path.substr(slash == std::string::npos ? 0 : slash + 1).c_str(), bytes.data(),
                   static_cast<int>(bytes.size()));
}

static bool readLine(const std::string &prompt, std::string &out)
{
    if (demoNext(prompt, out))
        return true; // scripted guided demo fed this line
    std::cout << prompt;
    std::cout.flush();
    out.clear();
    while (web_line_pending() == 0)
        emscripten_sleep(25);
    char buf[512];
    const int n = web_line_take((int)(uintptr_t)buf, (int)sizeof buf);
    if (n > 0)
        out.assign(buf, (std::string::size_type)n);
    while (!out.empty() && (out.back() == '\r' || out.back() == ' '))
        out.pop_back();
    return true;
}
#else
/// Read one line; returns false on EOF (input exhausted).
static bool readLine(const std::string &prompt, std::string &out)
{
    if (demoNext(prompt, out))
        return true; // scripted guided demo fed this line
    std::cout << prompt;
    std::cout.flush();
    if (!std::getline(std::cin, out))
        return false; // EOF -> caller exits gracefully
    while (!out.empty() && (out.back() == '\r' || out.back() == ' '))
        out.pop_back();
    return true;
}
#endif

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
        std::cout << red(L("  Please enter a whole number.\n"));
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
        std::cout << red(L("  Please enter a number.\n"));
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
        std::cout << red(L("  Invalid date - use YYYY-MM-DD (e.g. 2026-11-09).\n"));
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
        std::cout << red(L("  Choice must be between ")) << minChoice << L(" and ") << maxChoice
                  << ".\n";
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
    std::cout << yellow(L("        Console edition | C++17 | hand-built data structures\n\n"));
}

static void printCarTableHeader()
{
    std::ostringstream oss;
    oss << "  " << std::left << std::setw(16) << L("NAME") << std::setw(12) << L("BRAND")
        << std::right << std::setw(10) << L("PRICE/DAY") << "  " << std::left << std::setw(12)
        << L("STATUS") << std::right << std::setw(6) << L("QUEUE");
    std::cout << bold(oss.str()) << "\n";
    std::cout << "  " << std::string(62, '-') << "\n";
}

static void printCarRow(const Car *car)
{
    std::string status = toString(car->status());
    if (status == "AVAILABLE")
        status = green(L(status.c_str()));
    else if (status == "MAINTENANCE")
        status = red(L(status.c_str()));
    else if (status == "RENTED")
        status = yellow(L(status.c_str()));

    std::ostringstream price;
    price << std::fixed << std::setprecision(1) << car->pricePerDay();

    std::cout << "  " << std::left << std::setw(16) << car->name() << std::setw(12) << car->brand()
              << std::right << std::setw(10) << price.str() << "  " << std::left << std::setw(12)
              << status << std::right << std::setw(4) << car->reservationCount() << std::setw(2)
              << L(" booking(s)") << "\n";
}

static void printFleet(RentalManager &system, bool sorted)
{
    std::size_t count = system.fleetSize();
    if (count == 0)
    {
        std::cout << yellow(L("\n  The fleet is empty. Ask the manager to add cars.\n"));
        return;
    }

    std::cout << "\n"
              << bold(L("  FLEET (") + std::to_string(count) + L(" cars") +
                      (sorted ? L(", sorted by name") : L(", insertion order")) + ")\n");
    printCarTableHeader();
    auto printRow = [](Car *car) { printCarRow(car); };
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
    const std::vector<PickupReminder> pickups =
        system.upcomingPickups(REMINDER_HORIZON_DAYS, Date::todaySerial());
    if (pickups.empty())
        return;

    std::cout << bold(cyan(L("\n===== PICKUP REMINDERS | next ") +
                           std::to_string(REMINDER_HORIZON_DAYS) + L(" days =====\n")));
    for (const PickupReminder &p : pickups)
    {
        std::string phrase;
        if (p.daysAway <= 0)
            phrase = L("today");
        else if (p.daysAway == 1)
            phrase = L("tomorrow");
        else
            phrase = L("in ") + std::to_string(p.daysAway) + L(" days");
        std::cout << "  #" << p.reservationId << " | " << analyticsPad(p.carName, 16) << " | "
                  << Date::toISO(p.start) << "  " << yellow(phrase);
        if (p.seriesId > 0)
            std::cout << yellow(L("  [series #") + std::to_string(p.seriesId) + "]");
        std::cout << "\n";
    }
    std::cout << yellow(L("  Review or cancel them in My reservations.\n"));
}

static void printReservations(const std::vector<Reservation *> &list)
{
    if (list.empty())
    {
        std::cout << yellow(L("\n  You have no reservations yet.\n"));
        return;
    }
    std::cout << bold(L("\n  YOUR RESERVATIONS\n"));
    std::cout << "  " << std::string(66, '-') << "\n";
    for (const Reservation *r : list)
    {
        std::cout << "  #" << r->id() << " | " << std::left << std::setw(16) << r->car()->name()
                  << " | " << Date::toISO(r->startDate()) << " -> " << Date::toISO(r->endDate())
                  << " (" << r->durationDays() << L(" day(s))");
        // (v3.0.0) Occurrences of a recurring series carry the shared
        // series tag, so the customer can cancel the rest in one step.
        if (r->seriesId() > 0)
            std::cout << yellow(L("  [series #") + std::to_string(r->seriesId()) + "]");
        std::cout << "\n";
    }
}

/// The logged-in customer's waitlist wishes (v2.6.0).
static void printWaitlist(const std::vector<WaitlistEntry *> &list)
{
    if (list.empty())
    {
        std::cout << yellow(L("\n  You are not waiting for any car.\n"));
        return;
    }
    std::cout << bold(L("\n  YOUR WAITLIST\n"));
    std::cout << "  " << std::string(60, '-') << "\n";
    for (const WaitlistEntry *e : list)
        std::cout << L("  wish #") << e->joinOrder() << " | " << std::left << std::setw(16)
                  << e->car()->name() << " | " << Date::toISO(e->startDate()) << " -> "
                  << Date::toISO(e->endDate()) << " (" << e->durationDays() << L(" day(s))\n");
    std::cout << yellow(L("  We auto-book you the moment a wished range frees up.\n"));
}

/* ============================================================
 *  Screens: authentication
 * ============================================================ */

static void registerScreen(RentalManager &system)
{
    std::cout << bold(cyan(L("\n--- Customer Registration ---\n")));
    std::string username, password;
    if (!readToken(L("  Username: "), username))
        return;
    if (!readToken(L("  Password (min 4 chars): "), password))
        return;

    if (system.registerCustomer(username, password))
        std::cout << green(L("  Account created. You can log in now.\n"));
    else if (system.userExists(username))
        std::cout << red(L("  Username already taken.\n"));
    else
        std::cout << red(L("  Password must be at least 4 characters.\n"));
}

static void loginScreen(RentalManager &system)
{
    std::cout << bold(cyan(L("\n--- Login ---\n")));
    std::string username, password;
    if (!readToken(L("  Username: "), username))
        return;
    if (!readToken(L("  Password: "), password))
        return;

    if (system.login(username, password))
    {
        std::cout << green(L("  Welcome, ") + username + " (" +
                           L(system.currentUserRole().c_str()) + ")!\n");
        // (v3.1.0) A customer with pickups due this week hears
        // about them the moment they sign in. Non-customers hold
        // no pickups, so the screen stays quiet for staff.
        printPickupReminders(system);
    }
    else if (system.userExists(username) && system.isUserBlocked(username))
        std::cout << red(L("  This account is blocked. Contact the manager.\n"));
    else if (system.userExists(username))
        std::cout << red(L("  Wrong password. 3 failed attempts block the account!\n"));
    else
        std::cout << red(L("  No such user.\n"));
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
static void printBookingEstimate(const Car *car, int start, int end, LoyaltyTier tier)
{
    const int days = end - start + 1;
    const PriceBreakdown q = quoteRental(days, car->pricePerDay(), tier);
    const std::string note = discountNote(q);
    std::cout << L("  Estimate: ") << days << L(" day(s) x ") << money(car->pricePerDay()) << " = "
              << money(q.base);
    if (!note.empty())
        std::cout << yellow("  (" + note + ")");
    std::cout << L("\n  Pay on delivery about ") << bold(money(q.total))
              << L(" — late fees are never discounted.\n");
}

/* ============================================================
 *  Loyalty card (customer option 10) and redeem flow (option 11)
 * ============================================================
 *  Pure renderers over the Loyalty.h rules: the progress bar to
 *  the next rank reuses the analytics bar primitives, so every
 *  number the customer sees is pinned by the unit tests.
 * ============================================================ */
static void printLoyaltyCard(const std::string &username, const LoyaltyProfile &p)
{
    std::cout << bold(cyan(L("\n===== LOYALTY CARD | ") + username + " =====\n"));
    std::cout << L("\n  Rank: ") << bold(L(toString(p.tier()))) << "\n";

    if (p.tier() == LoyaltyTier::Platinum)
    {
        std::cout << L("  You hold the top rank of the program.\n");
    }
    else
    {
        const LoyaltyTier next = nextTier(p.tier());
        const int span = tierThreshold(next) - tierThreshold(p.tier());
        const int done = p.lifetimePoints - tierThreshold(p.tier());
        std::cout << "   " << analyticsBar(done, span, 28) << "  " << p.pointsToNextTier()
                  << L(" points to ") << L(toString(next)) << "\n";
    }

    std::ostringstream rate;
    rate << std::fixed << std::setprecision(2) << tierMultiplier(p.tier());
    std::cout << L("  Points available  : ") << p.points << L("\n  Lifetime points   : ")
              << p.lifetimePoints << L("\n  Completed rentals : ") << p.completedRentals
              << L("\n  Earning rate      : x") << rate.str()
              << L(" - 1 point per 10.00 spent, credited on return\n"
                   "  Redemption        : 10 points = 1.00 off your bill"
                   " (min 50, steps of 10)\n");
}

static void customerMenu(RentalManager &system, bool &running)
{
    std::cout << bold(cyan(L("\n===== CUSTOMER MENU | ") + system.currentUsername() +
                           L(" | balance due: ") + money(system.currentCustomerBalance()) +
                           " =====\n"));
    std::cout << L(
        "  1. View fleet\n  2. Search car by name\n  3. My reservations\n  4. Make a reservation\n "
        " 5. Find cars free for a date range\n  6. Join a waitlist\n  7. My waitlist (leave)\n  8. "
        "Renew a rental\n  9. Pay my bill\n 10. My loyalty card\n 11. Redeem points on my bill\n "
        "12. Logout\n 13. Change a booking (move dates)\n")

        ;

    switch (readMenuChoice(L("  Choice: "), 1, 13))
    {
    case 1:
        printFleet(system, false);
        break;
    case 2:
    {
        std::string name;
        if (!readToken(L("  Car name: "), name))
            return;
        Car *car = system.searchCar(name);
        if (car)
        {
            std::cout << bold(L("\n  FOUND:\n"));
            printCarRow(car);
        }
        else
            std::cout << red(L("  No car named \"") + name + "\".\n");
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
            if (!readLine(L("  Cancel a whole series? enter its #id (empty = stay): "), answer) ||
                answer.empty())
                break;
            int seriesId = 0;
            try
            {
                seriesId = std::stoi(answer);
            }
            catch (...)
            {
                std::cout << red(L("  Not a series id.\n"));
                break;
            }
            std::string error;
            const int removed = system.cancelRecurringSeries(seriesId, error);
            if (removed > 0)
                std::cout << green(L("  Series #") + std::to_string(seriesId) + ": " +
                                   std::to_string(removed) +
                                   L(" upcoming booking(s) cancelled.\n"));
            else
                std::cout << red("  " + error + "\n");
        }
        break;
    }
    case 4:
    {
        std::string name, error;
        int start, end;
        if (!readToken(L("  Car name: "), name))
            return;
        if (!readDate(L("  Start date (YYYY-MM-DD): "), start))
            return;
        if (!readDate(L("  End date   (YYYY-MM-DD): "), end))
            return;

        // (v3.0.0) Optional recurrence: a series books the same car
        // and duration again one plan step later, as many times as
        // requested — atomically, or not at all.
        int repeat = 0;
        if (!readInt(L("  Repeat: 0 none, 1 weekly, 2 biweekly, 3 monthly: "), repeat))
            return;

        if (repeat == 0)
        {
            if (system.makeReservation(name, start, end, error))
            {
                std::cout << green(L("  Reservation confirmed! Pay on delivery.\n"));
                // (v3.0.0) Money preview with the CURRENT rank.
                if (Car *booked = system.searchCar(name))
                    printBookingEstimate(booked, start, end, system.myLoyalty().tier());
            }
            else
            {
                std::cout << red(L("  Reservation failed: ") + error + "\n");
                if (error.find("overlap") != std::string::npos)
                    std::cout << yellow(L("  Tip: join the waitlist (option 6) and we book you\n   "
                                          "    automatically when the range frees up.\n"));
            }
            break;
        }

        RecurrencePlan plan;
        if (!planFromChoice(repeat, plan))
        {
            std::cout << red(L("  Choice must be between 0 and 3.\n"));
            break;
        }
        int times;
        if (!readInt(L("  Book it how many times (2-8): "), times))
            return;

        int seriesId = 0;
        if (system.makeRecurringReservation(name, start, end, plan, times, seriesId, error))
        {
            std::cout << green(L("  Series #") + std::to_string(seriesId) + L(" booked: ") +
                               std::to_string(times) + L(" x ") + L(toString(plan)) + L(" on ") +
                               name + "\n");
            const int durationDays = end - start + 1;
            for (int i = 0; i < times; ++i)
            {
                const int s = occurrenceStart(plan, start, i);
                std::cout << L("    pickup ") << (i + 1) << ": " << Date::toISO(s) << " -> "
                          << Date::toISO(s + durationDays - 1) << "\n";
            }
            // (v3.0.0) Per-occurrence money preview + series total.
            if (Car *booked = system.searchCar(name))
            {
                const PriceBreakdown q =
                    quoteRental(durationDays, booked->pricePerDay(), system.myLoyalty().tier());
                std::cout << L("  Estimate per pickup: ") << durationDays << L(" day(s) x ")
                          << money(booked->pricePerDay()) << " = " << money(q.base);
                if (q.discount > 0.0)
                    std::cout << yellow(" -> " + money(q.total) + L(" after ") +
                                        discountNote(q).substr(discountNote(q).find('(')));
                std::cout << L("\n  Series total about ") << bold(money(q.total * times))
                          << L(" (late fees are never discounted).\n");
            }
            std::cout << yellow(L("  The whole series is cancelled at once from\n       My "
                                  "reservations (option 3) — just give its #id.\n"));
        }
        else
            std::cout << red(L("  Series booking failed: ") + error + "\n");
        break;
    }
    case 5:
    {
        int start, end;
        if (!readDate(L("  Start date (YYYY-MM-DD): "), start))
            return;
        if (!readDate(L("  End date   (YYYY-MM-DD): "), end))
            return;
        std::vector<Car *> freeCars = system.findAvailableCars(start, end);
        if (freeCars.empty())
        {
            std::cout << red(L("  No car is free for ") + Date::toISO(start) + " -> " +
                             Date::toISO(end) + ".\n");
            std::cout << yellow(L("  Tip: join a car's waitlist (option 6) to be\n       "
                                  "auto-booked when a window opens up.\n"));
            break;
        }
        std::cout << bold(L("\n  FREE FOR ") + Date::toISO(start) + " -> " + Date::toISO(end) +
                          " (" + std::to_string(freeCars.size()) + L(" car(s))\n"));
        printCarTableHeader();
        for (const Car *car : freeCars)
            printCarRow(car);
        break;
    }
    case 6:
    {
        std::string name, error;
        int start, end;
        if (!readToken(L("  Car name: "), name))
            return;
        if (!readDate(L("  Desired start (YYYY-MM-DD): "), start))
            return;
        if (!readDate(L("  Desired end   (YYYY-MM-DD): "), end))
            return;
        if (system.joinWaitlist(name, start, end, error))
            std::cout << green(L("  You are on the waitlist for \"") + name +
                               L("\". We book you automatically when it frees up.\n"));
        else
            std::cout << red(L("  Waitlist failed: ") + error + "\n");
        break;
    }
    case 7:
    {
        printWaitlist(system.myWaitlist());
        std::string name;
        if (!readLine(L("  Car to leave (empty = stay): "), name) || name.empty())
            break;
        std::string error;
        if (system.leaveWaitlist(name, error))
            std::cout << green(L("  You left the waitlist of \"") + name + L("\".\n"));
        else
            std::cout << red("  " + error + "\n");
        break;
    }
    case 8:
    {
        std::string name, error;
        int days;
        if (!readToken(L("  Car name: "), name))
            return;
        if (!readInt(L("  Extra days: "), days))
            return;
        if (system.renewRental(name, days, error))
            std::cout << green(L("  Rental extended by ") + std::to_string(days) +
                               L(" day(s). Extra charges apply at return.\n"));
        else
            std::cout << red(L("  Renewal failed: ") + error + "\n");
        break;
    }
    case 9:
    {
        double owed = system.currentCustomerBalance();
        if (owed <= 0.0)
        {
            std::cout << green(L("  Your balance is clear. Nothing to pay.\n"));
        }
        else
        {
            double paid = system.payBill();
            std::cout << green(L("  Receipt: paid ") + money(paid) + L(". Thank you!\n"));
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
        std::cout << bold(cyan(L("\n--- REDEEM LOYALTY POINTS ---\n")));
        std::cout << L("  Points available: ") << profile.points << L("  |  bill due: ")
                  << money(owed) << "\n";
        if (owed <= 0.0)
        {
            std::cout << yellow(L("  Your bill is clear - points are redeemed against a bill,\n    "
                                  "   so come back after your next rental.\n"));
            break;
        }
        int points;
        if (!readInt(L("  Points to redeem (min 50, steps of 10): "), points))
            return;
        std::string error;
        const double credited = system.redeemPoints(points, error);
        if (credited >= 0.0)
            std::cout << green(L("  Credit of ") + money(credited) + L(" applied. Bill now: ") +
                               money(system.currentCustomerBalance()) + L(", points left: ") +
                               std::to_string(system.myLoyalty().points) + ".\n");
        else
            std::cout << red(L("  Redemption failed: ") + error + "\n");
        break;
    }
    case 12:
        system.logout();
        std::cout << yellow(L("  Logged out.\n"));
        break;
    case 13:
    {
        // (v3.2.0) Change of plans: move ONE booking to new dates or
        // shift a whole series. The facade lifts the booking out of
        // the schedule first, so its old slot never stands in the
        // way of its own new dates; a refusal leaves everything
        // untouched. Reminders (v3.1.0) are a read-only view, so
        // they follow the new dates automatically.
        std::cout << bold(cyan(L("\n--- CHANGE A BOOKING ---\n")));
        std::string mode, error;
        if (!readToken(L("  Move a single booking (1) or a whole series (2): "), mode))
            return;
        if (mode == "1")
        {
            std::string idText;
            if (!readToken(L("  Booking #id: "), idText))
                return;
            int id = 0;
            try
            {
                id = std::stoi(idText);
            }
            catch (...)
            {
                std::cout << red(L("  Not a booking id.\n"));
                break;
            }
            int newStart, newEnd;
            if (!readDate(L("  New start (YYYY-MM-DD): "), newStart))
                return;
            if (!readDate(L("  New end   (YYYY-MM-DD): "), newEnd))
                return;
            const std::vector<Reservation *> mine = system.myReservations();
            Reservation *target = nullptr;
            for (Reservation *r : mine)
                if (r->id() == id)
                    target = r;
            if (system.rescheduleReservation(id, newStart, newEnd, Date::todaySerial(), error))
            {
                std::cout << green(L("  Booking #") + std::to_string(id) + L(" moved to ") +
                                   Date::toISO(newStart) + " -> " + Date::toISO(newEnd) + ".\n");
                if (target) // money preview with the CURRENT rank
                    printBookingEstimate(target->car(), newStart, newEnd,
                                         system.myLoyalty().tier());
            }
            else
                std::cout << red(L("  Move failed: ") + error + "\n");
        }
        else if (mode == "2")
        {
            std::string idText;
            if (!readToken(L("  Series #id: "), idText))
                return;
            int seriesId = 0;
            try
            {
                seriesId = std::stoi(idText);
            }
            catch (...)
            {
                std::cout << red(L("  Not a series id.\n"));
                break;
            }
            int newNext;
            if (!readDate(L("  New date for the next pickup (YYYY-MM-DD): "), newNext))
                return;
            const int moved =
                system.rescheduleSeries(seriesId, newNext, Date::todaySerial(), error);
            if (moved > 0)
                std::cout << green(L("  Series #") + std::to_string(seriesId) + ": " +
                                   std::to_string(moved) +
                                   L(" upcoming booking(s) moved - reminders "
                                     "follow the new dates.\n"));
            else
                std::cout << red(L("  Move failed: ") + error + "\n");
        }
        else
            std::cout << red(L("  Choose 1 (single booking) or 2 (series).\n"));
        break;
    }
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
    std::cout << bold(cyan(L("\n===== STAFF MENU | ") + system.currentUsername() + " =====\n"));
    std::cout << L(
        "  1. View fleet\n  2. Search car by name\n  3. Deliver car to customer\n  4. Process car "
        "return (with penalty)\n  5. Send car to maintenance\n  6. Finish maintenance\n  7. Show "
        "maintenance history\n  8. Serve waitlist now\n  9. Logout\n")

        ;

    switch (readMenuChoice(L("  Choice: "), 1, 9))
    {
    case 1:
        printFleet(system, false);
        break;
    case 2:
    {
        std::string name;
        if (!readToken(L("  Car name: "), name))
            return;
        Car *car = system.searchCar(name);
        if (car)
        {
            std::cout << bold(L("\n  FOUND:\n"));
            printCarRow(car);
        }
        else
            std::cout << red(L("  No car named \"") + name + L("\".\n"));
        break;
    }
    case 3:
    {
        std::string name, error;
        if (!readToken(L("  Car name: "), name))
            return;
        if (system.deliverCar(name, error))
            std::cout << green(L("  Car handed over. Drive safe!\n"));
        else
            std::cout << red(L("  Delivery failed: ") + error + "\n");
        break;
    }
    case 4:
    {
        std::string name, error;
        int day;
        if (!readToken(L("  Car name: "), name))
            return;
        if (!readDate(L("  Actual return date (YYYY-MM-DD): "), day))
            return;
        if (system.returnCar(name, day, error))
        {
            std::cout << green(L("  Return processed and invoiced.\n"));
            // (v3.0.0) The discount the return earned, if any.
            const std::string discount = system.lastDiscountNote();
            if (!discount.empty())
                std::cout << green(L("  Discount applied: ") + discount + "\n");
            // (v2.9.0) A threshold crossed on this return is worth
            // announcing at the desk.
            const std::string rankUp = system.lastLoyaltyRankUp();
            if (!rankUp.empty())
                std::cout << yellow(L("  Rank up! ") + rankUp +
                                    L(" - the new multiplier applies from the next rental.\n"));
        }
        else
            std::cout << red(L("  Return failed: ") + error + "\n");
        break;
    }
    case 5:
    {
        std::string name, desc, error;
        int maintDay;
        double cost;
        if (!readToken(L("  Car name: "), name))
            return;
        if (!readDate(L("  Date (YYYY-MM-DD): "), maintDay))
            return;
        if (!readLine(L("  Description: "), desc) || desc.empty())
            desc = "general service";
        if (!readDouble(L("  Cost: "), cost))
            return;
        if (system.recordMaintenance(name, Date::toISO(maintDay), desc, cost, error))
            std::cout << yellow(L("  Car moved to MAINTENANCE. Log saved.\n"));
        else
            std::cout << red(L("  Failed: ") + error + "\n");
        break;
    }
    case 6:
    {
        std::string name;
        if (!readToken(L("  Car name: "), name))
            return;
        if (system.finishMaintenance(name))
            std::cout << green(L("  Maintenance finished; car is back in service.\n"));
        else
            std::cout << red(L("  Failed: car not found or not in maintenance.\n"));
        break;
    }
    case 7:
    {
        std::string name;
        if (!readToken(L("  Car name: "), name))
            return;
        Car *car = system.searchCar(name);
        if (!car)
        {
            std::cout << red(L("  No car named \"") + name + L("\".\n"));
            break;
        }
        const auto &log = car->maintenanceLog();
        if (log.empty())
        {
            std::cout << green(L("  No maintenance recorded for this car.\n"));
            break;
        }
        std::cout << bold(L("\n  MAINTENANCE LOG | ") + car->name() + "\n");
        for (const auto &rec : log)
            std::cout << "  " << rec.date << " | " << rec.description << L(" | cost ") << rec.cost
                      << "\n";
        break;
    }
    case 8:
    {
        std::string name, error;
        if (!readToken(L("  Car name: "), name))
            return;
        if (system.serveWaitlist(name, error))
        {
            Car *car = system.searchCar(name);
            const Reservation *r = car ? car->currentReservation() : nullptr;
            std::string who = r ? r->customer()->username() : "?";
            std::cout << green(L("  Booked ") + who + L(" from the waitlist.\n"));
        }
        else
            std::cout << red(L("  Serve failed: ") + error + "\n");
        break;
    }
    case 9:
        system.logout();
        std::cout << yellow(L("  Logged out.\n"));
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
    std::cout << bold(L("\n  FLEET STATISTICS\n"));
    std::cout << "  " << std::string(50, '-') << "\n";

    auto row = [](const std::string &label, const std::string &value)
    {
        std::cout << "  " << std::left << std::setw(24) << label << std::right << std::setw(26)
                  << value << "\n";
    };

    row(L("Fleet size"), std::to_string(s.totalCars) + L(" car(s)"));
    row(L("Available"), std::to_string(s.availableCars));
    row(L("Reserved"), std::to_string(s.reservedCars));
    row(L("Rented"), std::to_string(s.rentedCars));
    row(L("Under maintenance"), std::to_string(s.maintenanceCars));
    row(L("Active bookings"), std::to_string(s.activeBookings));
    std::ostringstream util;
    util << std::fixed << std::setprecision(1) << s.utilization << "%";
    row(L("Fleet utilization"), util.str());
    row(L("Gross income"), money(s.grossIncome));
    row(L("Maintenance spend"), money(s.maintenanceSpend));
    row(L("NET revenue"), money(s.netRevenue));
    row(L("Top earner"), s.topEarner.empty() ? "-" : s.topEarner);
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

    std::cout << bold(cyan(L("\n===== FLEET ANALYTICS | ") + Date::toISO(today) + " =====\n"));

    const auto drawBars =
        [](const std::string &title, const std::vector<NamedCount> &slices, std::size_t labelWidth)
    {
        std::cout << "\n  " << bold(title) << "\n";
        int maxCount = 0;
        for (const NamedCount &slice : slices)
            maxCount = std::max(maxCount, slice.count);
        for (const NamedCount &slice : slices)
            std::cout << "   " << analyticsPad(slice.label, labelWidth) << "  "
                      << analyticsBar(slice.count, maxCount) << " " << slice.count << "\n";
    };

    drawBars(L("Fleet composition"), report.fleetByStatus, 13);
    drawBars(L("Fleet by daily-rate bucket"), report.priceBuckets, 8);

    std::cout << "\n  " << bold(L("Active bookings per car")) << "\n";
    if (report.bookingsPerCar.empty())
    {
        std::cout << L("   (no live bookings)\n");
    }
    else
    {
        const int maxBookings = report.bookingsPerCar.front().count;
        for (const NamedCount &row : report.bookingsPerCar)
            std::cout << "   " << analyticsPad(row.label, 16) << "  "
                      << analyticsBar(row.count, maxBookings, 16) << " " << row.count << "\n";
    }

    std::cout << "\n  " << bold(L("Waitlist depth per car")) << "\n";
    if (report.waitlistPerCar.empty())
    {
        std::cout << L("   (nobody is waiting)\n");
    }
    else
    {
        const int maxWaiting = report.waitlistPerCar.front().count;
        for (const NamedCount &row : report.waitlistPerCar)
            std::cout << "   " << analyticsPad(row.label, 16) << "  "
                      << analyticsBar(row.count, maxWaiting, 16) << " " << row.count << "\n";
    }

    std::cout << "\n  " << bold(L("Upcoming pickups")) << "\n";
    if (report.upcoming.empty())
    {
        std::cout << L("   (nothing scheduled)\n");
    }
    else
    {
        for (const UpcomingPickup &p : report.upcoming)
        {
            std::string when;
            if (p.daysAway == 0)
                when = L("today");
            else if (p.daysAway == 1)
                when = L("tomorrow");
            else
                when = L("in ") + std::to_string(p.daysAway) + L(" days");
            std::cout << "   " << p.startDate << "  " << analyticsPad(p.carName, 16) << "  "
                      << p.customer << "  (" << when << ")\n";
        }
    }

    if (report.overdueReturns > 0)
        std::cout << yellow("\n  ! " + std::to_string(report.overdueReturns) +
                            L(" booking(s) past their end date — chase up the returns.\n"));
    if (report.totalWaitlisted > 0)
        std::cout << yellow("  ! " + std::to_string(report.totalWaitlisted) +
                            L(" customer(s) on a waitlist — serving one after every return.\n"));
    std::cout << "\n  " << report.totalActiveBookings << L(" live booking(s) fleet-wide.\n");
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
        std::cout << yellow(L("  The audit trail is empty.\n"));
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

    std::cout << bold(cyan(L("\n  AUDIT TRAIL — newest ") + std::to_string(tail.size()) +
                           L(" of ") + std::to_string(system.auditLog().size()) +
                           L(" entry(ies)\n")));
    std::cout << "  DATE       ACTOR        ACTION"
                 "               RESULT  DETAIL\n"
                 "  ---------------------------------------------------------"
                 "------------\n";
    for (const AuditEntry &e : tail)
    {
        const std::string line = "  " + pad(e.date, 10) + " " + pad(e.actor, 12) + " " +
                                 pad(e.action, 20) + "  " +
                                 (e.allowed ? std::string("ok     ") : std::string("DENIED ")) +
                                 e.detail.substr(0, 30) + "\n";
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
    std::cout << bold(cyan(L("\n===== LOYALTY PROGRAM OVERVIEW =====\n")));
    if (o.totalMembers == 0)
    {
        std::cout << yellow(L("\n  No customers are enrolled yet.\n"));
        return;
    }

    const char *tierNames[4] = {"Bronze", "Silver", "Gold", "Platinum"};
    int maxMembers = 0;
    for (const int count : o.membersByTier)
        maxMembers = std::max(maxMembers, count);
    std::cout << L("\n  Members per rank\n");
    for (int i = 0; i < 4; ++i)
        std::cout << "   " << analyticsPad(L(tierNames[i]), 9) << "  "
                  << analyticsBar(o.membersByTier[i], maxMembers, 20) << " " << o.membersByTier[i]
                  << "\n";

    std::cout << L("\n  Total members      : ") << o.totalMembers << L("\n  Outstanding points : ")
              << o.outstandingPoints << L("\n  Potential credits  : ") << money(o.pointsLiability)
              << L("\n  Top member         : ");
    if (o.topMember.empty())
        std::cout << "-\n";
    else
        std::cout << o.topMember << " (" << o.topMemberLifetime << L(" lifetime points)\n");

    // (v2.9.0) Recent promotions, straight from the audit trail —
    // the five newest loyalty.rankup entries, still chronological.
    std::vector<AuditEntry> rankUps;
    for (const AuditEntry &e : system.auditLog().all())
        if (e.action == "loyalty.rankup")
            rankUps.push_back(e);
    if (rankUps.size() > std::size_t(5))
        rankUps.erase(rankUps.begin(), rankUps.end() - 5);
    std::cout << L("\n  Recent rank-ups\n");
    if (rankUps.empty())
        std::cout << L("   none yet\n");
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
    std::cout << bold(cyan(L("\n===== MONTHLY REPORT | ") + r.month + " =====\n"));
    if (r.rentalsCompleted == 0)
    {
        std::cout << yellow(L("\n  No rentals were completed in ") + r.month + L(".\n"));
        return;
    }

    std::cout << L("\n  Rentals completed : ") << r.rentalsCompleted
              << L("\n  Revenue booked    : ") << money(r.revenue) << L("   (discounts given: ")
              << money(r.discountGiven) << L(" | late fees: ") << money(r.lateFees) << ")\n"
              << L("\n  Distinct customers: ") << r.distinctCustomers << "\n";

    int maxCarRentals = 0;
    for (const CarRevenue &car : r.perCar)
        maxCarRentals = std::max(maxCarRentals, car.rentals);
    std::cout << "\n  " << bold(L("Revenue by car")) << "\n";
    for (const CarRevenue &car : r.perCar)
        std::cout << "   " << analyticsPad(car.name, 16) << "  "
                  << analyticsBar(car.rentals, maxCarRentals, 16) << " " << car.rentals
                  << L(" rental(s), ") << money(car.revenue) << "\n";

    int maxDayReturns = 0;
    for (const NamedCount &day : r.perDay)
        maxDayReturns = std::max(maxDayReturns, day.count);
    std::cout << "\n  " << bold(L("Returns per day")) << "\n";
    for (const NamedCount &day : r.perDay)
        std::cout << "   " << analyticsPad(day.label, 4) << "  "
                  << analyticsBar(day.count, maxDayReturns, 16) << " " << day.count << "\n";

    char day[4];
    std::snprintf(day, sizeof day, "%02d", r.topDay);
    std::cout << L("\n  Busiest day: ") << day << " (" << r.topDayRentals << L(" return(s))\n");
}

static void managerMenu(RentalManager &system, bool &running)
{
    std::cout << bold(cyan(L("\n===== MANAGER MENU | ") + system.currentUsername() + " =====\n"));
    std::cout << L(
        "  1. Add car\n  2. View fleet (insertion order)\n  3. View fleet (sorted by name - AVL "
        "index)\n  4. Search fleet by keyword\n  5. Fleet statistics\n  6. Fleet analytics "
        "dashboard\n  7. Audit log (who did what)\n  8. Loyalty program overview\n  9. Income "
        "report\n 10. Export CSV & JSON reports\n 11. Block user\n 12. Unblock user\n 13. Create "
        "staff account\n 14. Save data to disk\n 15. Save & logout\n 16. Exit program\n 17. "
        "Monthly report (revenue & bookings)\n 18. Save monthly report as PNG\n"
        " 19. Save loyalty overview as PNG\n 20. Save analytics dashboard as "
        "PNG\n 21. Save all three reports as one PDF\n")

        ;

    switch (readMenuChoice(L("  Choice: "), 1, 21))
    {
    case 1:
    {
        std::string name, brand;
        double price;
        if (!readToken(L("  Car name: "), name))
            return;
        if (!readToken(L("  Brand: "), brand))
            return;
        if (!readDouble(L("  Price per day: "), price))
            return;
        if (system.addCar(new Car(name, brand, price)))
            std::cout << green(L("  Car added to the fleet.\n"));
        else
            std::cout << red(L("  A car with this name already exists.\n"));
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
        if (!readToken(L("  Keyword (name or brand): "), keyword))
            return;
        std::vector<Car *> matches = system.searchFleet(keyword);
        if (matches.empty())
        {
            std::cout << red(L("  Nothing matches \"") + keyword + "\".\n");
            break;
        }
        std::cout << bold(L("\n  MATCHES (") + std::to_string(matches.size()) + ")\n");
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
        const bool bookingsOk = system.exportBookingReport("reports/bookings_report.csv");
        const bool loyaltyOk = system.exportLoyaltyReport("reports/loyalty_report.csv");
        const bool jsonOk = exportJsonReport(system, "reports/report.json");
        if (fleetOk && bookingsOk && loyaltyOk && jsonOk)
            std::cout << green(std::string(L("  Reports written:\n")) +
                               "    reports/fleet_report.csv\n"
                               "    reports/bookings_report.csv\n"
                               "    reports/loyalty_report.csv\n"
                               "    reports/report.json\n");
        else
            std::cout << red(L("  Could not write the report files.\n"));
        break;
    }
    case 11:
    {
        std::string user;
        if (!readToken(L("  Username to block: "), user))
            return;
        if (system.blockUser(user))
            std::cout << yellow(L("  User \"") + user + L("\" blocked.\n"));
        else
            std::cout << red(L("  No such user.\n"));
        break;
    }
    case 12:
    {
        std::string user;
        if (!readToken(L("  Username to unblock: "), user))
            return;
        if (system.unblockUser(user))
            std::cout << green(L("  User \"") + user + L("\" unblocked.\n"));
        else
            std::cout << red(L("  No such user.\n"));
        break;
    }
    case 13:
    {
        std::string user, pass, role;
        if (!readToken(L("  New username: "), user))
            return;
        if (!readToken(L("  Password (min 4 chars): "), pass))
            return;
        if (!readToken(L("  Role (Staff / Manager): "), role))
            return;
        if (system.registerStaffAccount(user, pass, role))
            std::cout << green(std::string("  ") + L(role.c_str()) + L(" account created.\n"));
        else
            std::cout << red(L("  Failed: check username is new, password >= 4 chars, role is "
                               "Staff or Manager.\n"));
        break;
    }
    case 14:
        if (g_demoOn)
            std::cout << yellow(L("  Guided demo: the fleet lives in memory — nothing to save.\n"));
        else
            std::cout << (system.saveData() ? green(L("  Data saved to ./data\n"))
                                            : red(L("  Could not write data files.\n")));
        break;
    case 15:
        if (!g_demoOn)
            system.saveData();
        system.logout();
        std::cout << (g_demoOn ? yellow(L("  Demo fleet kept in memory. Logged out.\n"))
                               : yellow(L("  Data saved. Logged out.\n")));
        break;
    case 16:
        if (!g_demoOn)
            system.saveData();
        std::cout << (g_demoOn ? yellow(L("  Demo fleet kept in memory. Goodbye!\n"))
                               : yellow(L("  Data saved. Goodbye!\n")));
        running = false;
        break;
    case 17:
    {
        // (v3.0.0) One month at a glance, then the same numbers as
        // a CSV next to the other reports.
        int year;
        if (!readInt(L("  Year (e.g. 2026): "), year))
            return;
        const int month = readMenuChoice(L("  Month (1-12): "), 1, 12);

        const MonthlyReport r = system.monthlyReport(year, month);
        renderMonthlyReport(r);
        if (r.rentalsCompleted > 0)
        {
            std::ostringstream name;
            name << "reports/monthly_" << r.month << ".csv";
            std::string error;
            if (system.exportMonthlyReport(year, month, name.str(), error))
                std::cout << green(L("  Saved to ") + name.str() + "\n");
            else
                std::cout << red(L("  Could not write ") + name.str() + ": " + error + "\n");
        }
        break;
    }
    case 18:
    {
        // (v3.7.0) The same month as a dependency-free PNG image:
        // hand-rolled encoder, no image library anywhere.
        int year;
        if (!readInt(L("  Year (e.g. 2026): "), year))
            return;
        const int month = readMenuChoice(L("  Month (1-12): "), 1, 12);
        const MonthlyReport r = system.monthlyReport(year, month);
        const std::string path = "reports/monthly_" + r.month + ".png";
        std::string error;
        if (png::writeMonthlyReportPng(r, year, month, path, error))
            std::cout << green(L("  Saved to ") + path + "\n");
#ifdef __EMSCRIPTEN__
        offerReportDownload(path); // browser demo: one-click download
#endif
        else std::cout << red(L("  Could not write ") + path + ": " + error + "\n");
        break;
    }
    case 19:
    {
        // (v3.8.0) The loyalty snapshot as a dependency-free PNG.
        const LoyaltyOverview o = system.loyaltyOverview();
        const std::string path = "reports/loyalty_overview.png";
        std::string error;
        if (png::writeLoyaltyOverviewPng(o, path, error))
            std::cout << green(L("  Saved to ") + path + "\n");
#ifdef __EMSCRIPTEN__
        offerReportDownload(path); // browser demo: one-click download
#endif
        else std::cout << red(L("  Could not write ") + path + ": " + error + "\n");
        break;
    }
    case 20:
    {
        // (v3.8.0) The analytics dashboard as a dependency-free PNG:
        // the same AnalyticsReport the console screen renders.
        const AnalyticsReport report = buildAnalytics(system, Date::todaySerial());
        const std::string path = "reports/dashboard.png";
        std::string error;
        if (png::writeDashboardPng(report, path, error))
            std::cout << green(L("  Saved to ") + path + "\n");
#ifdef __EMSCRIPTEN__
        offerReportDownload(path); // browser demo: one-click download
#endif
        else std::cout << red(L("  Could not write ") + path + ": " + error + "\n");
        break;
    }
    case 21:
    {
        // (v3.9.0) All three reports consolidated into one PDF.
        int year;
        if (!readInt(L("  Year (e.g. 2026): "), year))
            return;
        const int month = readMenuChoice(L("  Month (1-12): "), 1, 12);
        const MonthlyReport r = system.monthlyReport(year, month);
        const std::string path = "reports/all_reports_" + r.month + ".pdf";
        std::string error;
        if (pdf::writeAllReportsPdf(r, year, month, system.loyaltyOverview(),
                                    buildAnalytics(system, Date::todaySerial()), path, error))
            std::cout << green(L("  Saved to ") + path + "\n");
#ifdef __EMSCRIPTEN__
        offerReportDownload(path); // browser demo: one-click download
#endif
        else std::cout << red(L("  Could not write ") + path + ": " + error + "\n");
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
    const cli::Options opts = cli::parse(argc, argv);
    if (!opts.error.empty())
    {
        std::cerr << "car_rental: " << opts.error << " — try --help\n";
        return 1;
    }

    // Plain output: the no-color.org convention (present and non-empty)
    // or the explicit --no-color flag.
    if (const char *nc = std::getenv("NO_COLOR"))
        if (*nc != '\0')
            g_colors = false;
    if (opts.noColor)
        g_colors = false;

#ifdef _WIN32
    // Persian UI strings are UTF-8; the legacy console needs the code
    // page flipped or every FA screen prints mojibake.
    SetConsoleOutputCP(65001);
#endif

    // The guided demo (--demo): build the scripted shift up front; from
    // here on readLine() pops its lines until the walkthrough ends.
    if (opts.demo)
    {
        g_demoOn = true;
        demoBuildScript();
    }

    // UI language (v3.6.0): --lang wins and persists, else the
    // persisted choice, else English — which keeps the CI contract
    // greps (--help, --demo markers) byte-stable. The guided demo
    // always plays in English unless --lang says otherwise: its
    // narration and the CI greps are part of the machine contract.
    if (!opts.lang.empty())
    {
        if (!i18n::setLangByName(opts.lang))
            std::cerr << "car_rental: unknown --lang '" << opts.lang
                      << "' (fa|en) — English kept\n";
        else if (!g_demoOn && !opts.showHelp && !opts.showVersion)
            i18n::persist();
    }
    else if (g_demoOn)
    {
        i18n::setLang(i18n::Lang::En);
    }
    else
    {
        i18n::loadPersisted();
    }

    if (opts.showVersion)
    {
        std::cout << "Car Rental Management System v" << APP_VERSION
                  << " — C++17, zero dependencies\n";
        return 0;
    }
    if (opts.showHelp)
    {
        std::cout << "Car Rental Management System v" << APP_VERSION
                  << "\n"
                     "\n"
                     "Usage: car_rental [option]\n"
                     "\n"
                     "  (no option)    start the interactive console app\n"
                     "  -v, --version  print version and exit\n"
                     "  -h, --help     show this help and exit\n"
                     "  --no-color     plain output (pipes, logs, CI)\n"
                     "  --demo         guided walkthrough: one full shift,\n"
                     "                 narrated, no typing (fresh in-memory fleet)\n"
                     "  --serve [port] read-only REST API over the loaded data\n"
                     "                 (default port 8080, binds 127.0.0.1)\n"
                     "\n"
                     "Plain output also follows the NO_COLOR environment\n"
                     "variable (no-color.org).\n"
                     "\n"
                     "Demo accounts (auto-seeded on first run):\n"
                     "  manager  admin / manager123\n"
                     "  staff    sara  / staff123\n"
                     "  customer ali   / customer123\n"
                     "\n"
                     "No compiler? The same binary runs in your browser:\n"
                     "  https://abolfazlrwm.github.io/Car-Rental-System/demo.html\n";
        return 0;
    }

    RentalManager system;
    bool running = true;

    printBanner();

    // Load persisted state; a fresh clone gets a ready-to-demo fleet.
    if (g_demoOn)
    {
        // The guided demo plays on a fresh in-memory fleet: ./data is
        // neither read nor written, so it can run anywhere, any time.
        system.seedDemoData();
        std::cout << yellow(L("  Guided demo fleet: fresh, in-memory — ./data is untouched.\n"));
    }
    else if (system.loadData())
        std::cout << green(L("  Loaded saved state from ./data\n"));
    else
    {
        system.seedDemoData();
        std::cout << yellow(L("  First run: demo fleet + accounts seeded.\n"));
        std::cout << L("    manager  -> admin / manager123\n    staff    -> sara / staff123\n    "
                       "customer -> ali  / customer123\n");
        if (i18n::lang() == i18n::Lang::En)
            std::cout << yellow("  Tip: run with --lang fa for the Persian interface.\n");
    }

    // (v3.7.0) Read-only REST API: --serve replaces the menu loop.
    if (opts.servePort > 0)
    {
        if (g_demoOn)
        {
            std::cout << red(L("  --serve and --demo cannot be combined.\n"));
            return 1;
        }
        std::string serveError;
        const int serveExit = rest::serve(system, opts.servePort, serveError);
        if (serveExit != 0)
            std::cerr << "car_rental: " << serveError << "\n";
        return serveExit == 0 ? 0 : 1;
    }

    while (running)
    {
        if (!system.isLoggedIn())
        {
            std::cout << bold(cyan(L("\n===== MAIN MENU =====\n")));
            std::cout << L("  1. Register (customer)\n  2. Login\n  3. Exit\n")

                ;

            switch (readMenuChoice(L("  Choice: "), 1, 3))
            {
            case 1:
                registerScreen(system);
                break;
            case 2:
                loginScreen(system);
                break;
            case 3:
                running = false;
                std::cout << cyan(L("  Goodbye!\n"));
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
    // (--demo never writes: its fleet was in-memory by design.)
    if (!g_demoOn)
        system.saveData();
    return 0;
}
