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

#include "manager_system/RentalManager.h"
#include "manager_system/Date.h"

/* Program version, reported by --version / --help. */
static const char *APP_VERSION = "2.3.0";

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
                  << " (" << r->durationDays() << " day(s))\n";
    }
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
        std::cout << green("  Welcome, " + username + " ("
                           + system.currentUserRole() + ")!\n");
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

static void customerMenu(RentalManager &system, bool &running)
{
    std::cout << bold(cyan("\n===== CUSTOMER MENU | " + system.currentUsername()
                           + " | balance due: " + money(system.currentCustomerBalance())
                           + " =====\n"));
    std::cout << "  1. View fleet\n"
                 "  2. Search car by name\n"
                 "  3. My reservations\n"
                 "  4. Make a reservation\n"
                 "  5. Renew a rental\n"
                 "  6. Pay my bill\n"
                 "  7. Logout\n";

    switch (readMenuChoice("  Choice: ", 1, 7))
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
        printReservations(system.myReservations());
        break;
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
        if (system.makeReservation(name, start, end, error))
            std::cout << green("  Reservation confirmed! Pay on delivery.\n");
        else
            std::cout << red("  Reservation failed: " + error + "\n");
        break;
    }
    case 5:
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
    case 6:
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
    case 7:
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
                 "  8. Logout\n";

    switch (readMenuChoice("  Choice: ", 1, 8))
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
            std::cout << green("  Return processed and invoiced.\n");
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

static void managerMenu(RentalManager &system, bool &running)
{
    std::cout << bold(cyan("\n===== MANAGER MENU | " + system.currentUsername() + " =====\n"));
    std::cout << "  1. Add car\n"
                 "  2. View fleet (insertion order)\n"
                 "  3. View fleet (sorted by name - AVL index)\n"
                 "  4. Search fleet by keyword\n"
                 "  5. Fleet statistics\n"
                 "  6. Income report\n"
                 "  7. Export CSV reports\n"
                 "  8. Block user\n"
                 "  9. Unblock user\n"
                 " 10. Create staff account\n"
                 " 11. Save data to disk\n"
                 " 12. Save & logout\n"
                 " 13. Exit program\n";

    switch (readMenuChoice("  Choice: ", 1, 13))
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
        system.printIncomeReport();
        break;
    case 7:
    {
        const bool fleetOk = system.exportFleetReport("reports/fleet_report.csv");
        const bool bookingsOk =
            system.exportBookingReport("reports/bookings_report.csv");
        if (fleetOk && bookingsOk)
            std::cout << green("  Reports written:\n"
                               "    reports/fleet_report.csv\n"
                               "    reports/bookings_report.csv\n");
        else
            std::cout << red("  Could not write the report files.\n");
        break;
    }
    case 8:
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
    case 9:
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
    case 10:
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
    case 11:
        std::cout << (system.saveData() ? green("  Data saved to ./data\n")
                                        : red("  Could not write data files.\n"));
        break;
    case 12:
        system.saveData();
        system.logout();
        std::cout << yellow("  Data saved. Logged out.\n");
        break;
    case 13:
        system.saveData();
        std::cout << yellow("  Data saved. Goodbye!\n");
        running = false;
        break;
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
