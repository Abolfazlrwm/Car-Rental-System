#include "RentalManager.h"

#include "Date.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

const double RentalManager::PENALTY_RATE = 1.5;

/* ============================================================
 *  Construction / destruction
 * ============================================================ */

RentalManager::RentalManager()
    : userIndex_(&RentalManager::stringHash, /*ownsValues=*/true),
      carIndex_(/*ownsValues=*/true),
      currentUser_(nullptr),
      reservationCounter_(0) {}

RentalManager::~RentalManager()
{
    clearAll();
}

void RentalManager::clearAll()
{
    // HashTable deletes users, AVL deletes cars, and each Car's
    // destructor deletes its reservations -> every allocation is
    // freed exactly once.
    carIndex_.clear();
    userIndex_.clear();
    carOrder_.clear();
    currentUser_ = nullptr;
}

/* ============================================================
 *  Helpers
 * ============================================================ */

std::size_t RentalManager::stringHash(const std::string &key)
{
    // Classic polynomial rolling hash: h = h*31 + c  (as used in Java).
    std::size_t h = 0;
    for (unsigned char c : key)
        h = h * 31 + c;
    return h;
}

Customer *RentalManager::asCustomer() const
{
    if (!currentUser_ || currentUser_->role() != "Customer")
        return nullptr;
    return static_cast<Customer *>(currentUser_);
}

/* ============================================================
 *  Audit trail
 * ============================================================ */

void RentalManager::audit(const std::string &action,
                          const std::string &detail, bool allowed) const
{
    auditLog_.record(Date::toISO(Date::todaySerial()),
                     currentUser_ ? currentUser_->username()
                                  : std::string("?"),
                     action, detail, allowed);
}

/// Lowercased copy used by the keyword search (ASCII only — car names
/// and brands are Latin by contract, see docs/DATA_FORMATS.md).
static std::string toLowerCopy(const std::string &text)
{
    std::string out = text;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c)
                   { return static_cast<char>(std::tolower(c)); });
    return out;
}

/* ============================================================
 *  Account management
 * ============================================================ */

bool RentalManager::registerCustomer(const std::string &username,
                                     const std::string &password)
{
    if (username.empty() || password.size() < 4)
        return false;
    if (userIndex_.contains(username))
        return false;

    Customer *user = new Customer(username);
    user->setPassword(password);
    userIndex_.insert(username, user);
    audit("customer.registered", username);
    return true;
}

bool RentalManager::registerStaffAccount(const std::string &username,
                                         const std::string &password,
                                         const std::string &role)
{
    if (username.empty() || password.size() < 4)
        return false;
    if (role != "Staff" && role != "Manager")
        return false;
    if (userIndex_.contains(username))
        return false;

    User *user = (role == "Staff") ? static_cast<User *>(new Staff(username))
                                   : static_cast<User *>(new Manager(username));
    user->setPassword(password);
    userIndex_.insert(username, user);
    audit("user.created", role + " " + username);
    return true;
}

bool RentalManager::login(const std::string &username,
                          const std::string &password)
{
    User **slot = userIndex_.find(username);
    if (!slot) // FIX: original dereferenced null here and segfaulted
    {
        audit("login.failed", username + " (unknown user)", false);
        return false;
    }

    User *user = *slot;
    if (user->isBlocked())
    {
        audit("login.denied", username + " (account blocked)", false);
        return false;
    }

    if (!user->verifyPassword(password))
    {
        user->registerFailedLogin(); // 3 strikes -> account blocked
        audit("login.failed", username, false);
        if (user->isBlocked())
            audit("account.autoblock",
                  username + " after 3 failed logins", false);
        return false;
    }

    user->resetFailedLogins();
    currentUser_ = user;
    audit("login.ok", user->role());
    return true;
}

void RentalManager::logout() { currentUser_ = nullptr; }

bool RentalManager::isLoggedIn() const { return currentUser_ != nullptr; }

std::string RentalManager::currentUserRole() const
{
    return currentUser_ ? currentUser_->role() : "";
}

std::string RentalManager::currentUsername() const
{
    return currentUser_ ? currentUser_->username() : "";
}

bool RentalManager::blockUser(const std::string &username)
{
    User **slot = userIndex_.find(username);
    if (!slot)
        return false;
    (*slot)->block();
    audit("user.blocked", username);
    return true;
}

bool RentalManager::unblockUser(const std::string &username)
{
    User **slot = userIndex_.find(username);
    if (!slot)
        return false;
    (*slot)->unblock();
    audit("user.unblocked", username);
    return true;
}

bool RentalManager::userExists(const std::string &username) const
{
    return userIndex_.contains(username);
}

bool RentalManager::isUserBlocked(const std::string &username) const
{
    User *const *slot = userIndex_.find(username);
    return slot && (*slot)->isBlocked();
}

/* ============================================================
 *  Fleet management
 * ============================================================ */

bool RentalManager::addCar(Car *car)
{
    if (!car || carIndex_.contains(car->name()))
    {
        delete car; // reject duplicates without leaking
        return false;
    }
    carIndex_.insert(car->name(), car);
    carOrder_.pushBack(car->name());
    audit("car.added", car->name());
    return true;
}

Car *RentalManager::searchCar(const std::string &name)
{
    Car **slot = carIndex_.find(name);
    return slot ? *slot : nullptr;
}

std::vector<Car *> RentalManager::findCarsByBrand(const std::string &brand) const
{
    std::vector<Car *> matches;
    carIndex_.inOrderForEach([&](const std::string &, Car *car)
                             {
                                 if (car->brand() == brand)
                                     matches.push_back(car);
                             });
    return matches;
}

void RentalManager::forEachCar(const std::function<void(Car *)> &fn) const
{
    carOrder_.forEach([&](const std::string &name)
                      {
                          const Car *const *slot = carIndex_.find(name);
                          if (slot)
                              fn(const_cast<Car *>(*slot));
                      });
}

void RentalManager::forEachCarSorted(const std::function<void(Car *)> &fn) const
{
    carIndex_.inOrderForEach([&](const std::string &, Car *car)
                             { fn(car); });
}

std::vector<Car *> RentalManager::findBookableCars() const
{
    std::vector<Car *> out;
    forEachCar([&](Car *car)
               {
                   if (car->isAvailableForBooking())
                       out.push_back(car);
               });
    return out;
}

std::vector<Car *> RentalManager::findCarsInPriceRange(double minPrice,
                                                       double maxPrice) const
{
    std::vector<Car *> out;
    carIndex_.inOrderForEach([&](const std::string &, Car *car)
                             {
                                 double rate = car->pricePerDay();
                                 if (rate >= minPrice && rate <= maxPrice)
                                     out.push_back(car);
                             });
    std::sort(out.begin(), out.end(),
              [](const Car *a, const Car *b)
              {
                  if (a->pricePerDay() != b->pricePerDay())
                      return a->pricePerDay() < b->pricePerDay();
                  return a->name() < b->name();
              });
    return out;
}

void RentalManager::forEachCarByPrice(const std::function<void(Car *)> &fn) const
{
    std::vector<Car *> fleet;
    carIndex_.inOrderForEach([&](const std::string &, Car *car)
                             { fleet.push_back(car); });
    std::sort(fleet.begin(), fleet.end(),
              [](const Car *a, const Car *b)
              {
                  if (a->pricePerDay() != b->pricePerDay())
                      return a->pricePerDay() < b->pricePerDay();
                  return a->name() < b->name();
              });
    for (Car *car : fleet)
        fn(car);
}

std::vector<Car *> RentalManager::searchFleet(const std::string &keyword) const
{
    std::vector<Car *> matches;
    const std::string needle = toLowerCopy(keyword);
    if (needle.empty())
        return matches;

    forEachCar([&](Car *car)
               {
                   if (toLowerCopy(car->name()).find(needle) != std::string::npos ||
                       toLowerCopy(car->brand()).find(needle) != std::string::npos)
                       matches.push_back(car);
               });
    return matches;
}

bool RentalManager::sendToMaintenance(const std::string &carName)
{
    Car *car = searchCar(carName);
    if (!car)
        return false;
    if (car->status() == CarStatus::Rented)
        return false; // cannot service a car that is out with a customer

    car->setStatus(CarStatus::Maintenance);
    audit("maintenance.sent", carName);
    return true;
}

bool RentalManager::finishMaintenance(const std::string &carName)
{
    Car *car = searchCar(carName);
    if (!car || car->status() != CarStatus::Maintenance)
        return false;

    car->setStatus(CarStatus::Available);
    car->refreshStatus(); // back to Reserved if bookings are queued
    audit("maintenance.finished", carName);
    return true;
}

/* ============================================================
 *  Customer operations
 * ============================================================ */

bool RentalManager::makeReservation(const std::string &carName, int startDate,
                                    int endDate, std::string &errorOut)
{
    Customer *customer = asCustomer();
    if (!customer)
    {
        errorOut = "Only logged-in customers can reserve.";
        return false;
    }

    Car *car = searchCar(carName);
    if (!car)
    {
        errorOut = "Car not found.";
        return false;
    }
    if (car->status() == CarStatus::Maintenance)
    {
        errorOut = "Car is under maintenance.";
        return false;
    }
    if (startDate < Date::MIN_SERIAL || startDate > endDate)
    {
        errorOut = "Invalid date range (need start >= 1970-01-01 and start <= end).";
        return false;
    }
    if (car->hasOverlap(startDate, endDate)) // double-booking guard
    {
        errorOut = "Dates overlap an existing reservation.";
        return false;
    }

    const int newId = ++reservationCounter_; // unique id == arrival order
    Reservation *r = new Reservation(customer, car, newId, newId,
                                     startDate, endDate);
    car->reserve(r);
    audit("reservation.made", carName + " " + Date::toISO(startDate)
                                  + ".." + Date::toISO(endDate));
    return true;
}

std::vector<Reservation *> RentalManager::myReservations() const
{
    std::vector<Reservation *> result;
    if (!currentUser_)
        return result;

    // The AVL in-order walk gives a stable, name-sorted listing.
    carIndex_.inOrderForEach([&](const std::string &, Car *car)
                             {
                                 for (Reservation *r : car->reservationsOf(currentUser_->username()))
                                     result.push_back(r);
                             });

    std::sort(result.begin(), result.end(),
              [](const Reservation *a, const Reservation *b)
              { return a->startDate() < b->startDate(); });
    return result;
}

bool RentalManager::renewRental(const std::string &carName, int extraDays,
                                std::string &errorOut)
{
    Customer *customer = asCustomer();
    if (!customer || extraDays < 1)
    {
        errorOut = "Invalid renewal request.";
        return false;
    }

    Car *car = searchCar(carName);
    if (!car || car->status() != CarStatus::Rented)
    {
        errorOut = "Car is not currently rented.";
        return false;
    }

    Reservation *current = car->currentReservation();
    if (!current || current->customer() != customer)
    {
        errorOut = "This rental does not belong to you.";
        return false;
    }
    if (car->reservationCount() > 1)
    {
        errorOut = "Cannot renew: other bookings are waiting for this car.";
        return false;
    }

    // FIX: the original printed "extended" but never touched the dates.
    current->extendEndDate(extraDays);
    return true;
}

double RentalManager::payBill()
{
    Customer *customer = asCustomer();
    if (!customer)
        return -1.0;
    double amount = customer->balance();
    customer->payBalance();
    return amount;
}

double RentalManager::currentCustomerBalance() const
{
    Customer *customer = asCustomer();
    return customer ? customer->balance() : 0.0;
}

/* ============================================================
 *  Staff operations
 * ============================================================ */

bool RentalManager::deliverCar(const std::string &carName, std::string &errorOut)
{
    Car *car = searchCar(carName);
    if (!car)
    {
        errorOut = "Car not found.";
        return false;
    }
    if (car->status() == CarStatus::Rented)
    {
        errorOut = "Car is already rented.";
        return false;
    }
    if (car->status() == CarStatus::Maintenance)
    {
        errorOut = "Finish maintenance before delivering.";
        return false;
    }

    Reservation *r = car->currentReservation();
    if (!r)
    {
        errorOut = "No reservation waiting for this car.";
        return false;
    }

    car->setStatus(CarStatus::Rented);
    audit("car.delivered", carName);
    return true;
}

bool RentalManager::returnCar(const std::string &carName, int actualReturnDay,
                              std::string &errorOut)
{
    Car *car = searchCar(carName);
    if (!car || car->status() != CarStatus::Rented)
    {
        errorOut = "Car is not currently rented.";
        return false;
    }

    Reservation *res = car->currentReservation();
    if (!res)
    {
        errorOut = "No active reservation found (data inconsistency).";
        return false;
    }

    // ---- invoice -----------------------------------------------------
    // Base charge: full booked duration at the daily price.
    double baseCharge = res->durationDays() * car->pricePerDay();
    double penalty = 0.0;

    int scheduledEnd = res->endDate();
    if (actualReturnDay > scheduledEnd)
    {
        int delayDays = actualReturnDay - scheduledEnd;
        // Late days cost 1.5x the daily rate.
        penalty = delayDays * car->pricePerDay() * PENALTY_RATE;
    }

    double total = baseCharge + penalty;

    // Ledger: income is recorded EXACTLY ONCE (at return time).
    car->addIncome(total);

    // The customer owes the money until they pay their bill.
    Customer *cust = res->customer();
    if (cust)
        cust->charge(total);

    // Close the booking and free it (Car does not delete it here —
    // ownership of a served booking moves to this function).
    (void)car->completeReservation();
    delete res;

    car->refreshStatus();
    audit("car.returned",
          carName + (penalty > 0.0 ? " (late penalty applied)" : ""));
    return true;
}

bool RentalManager::recordMaintenance(const std::string &carName,
                                      const std::string &date,
                                      const std::string &description,
                                      double cost, std::string &errorOut)
{
    Car *car = searchCar(carName);
    if (!car)
    {
        errorOut = "Car not found.";
        return false;
    }
    if (car->status() == CarStatus::Rented)
    {
        errorOut = "Car is rented; it cannot be serviced now.";
        return false;
    }
    if (cost < 0)
    {
        errorOut = "Cost cannot be negative.";
        return false;
    }

    car->addMaintenanceLog(date, description, cost);
    car->setStatus(CarStatus::Maintenance);
    audit("maintenance.logged", carName);
    return true;
}

void RentalManager::printIncomeReport() const
{
    double totalIncome = 0.0, totalCosts = 0.0;
    std::cout << "\n  ----------------- INCOME REPORT -----------------\n";
    std::cout << "  " << std::left << std::setw(16) << "CAR"
              << std::right << std::setw(12) << "INCOME"
              << std::setw(14) << "MAINTENANCE"
              << std::setw(12) << "NET"
              << "\n";
    std::cout << "  " << std::string(54, '-') << "\n";

    forEachCar([&](Car *car)
               {
                   totalIncome += car->totalIncome();
                   totalCosts += car->maintenanceCost();
                   std::cout << "  " << std::left << std::setw(16) << car->name()
                             << std::right << std::fixed << std::setprecision(2)
                             << std::setw(12) << car->totalIncome()
                             << std::setw(14) << car->maintenanceCost()
                             << std::setw(12) << car->netRevenue()
                             << "\n";
               });

    std::cout << "  " << std::string(54, '-') << "\n";
    std::cout << "  " << std::left << std::setw(28) << "  Gross income"
              << std::right << std::fixed << std::setprecision(2) << totalIncome << "\n";
    std::cout << "  " << std::left << std::setw(28) << "  Maintenance costs"
              << std::right << std::fixed << std::setprecision(2) << totalCosts << "\n";
    std::cout << "  " << std::left << std::setw(28) << "  NET revenue"
              << std::right << std::fixed << std::setprecision(2)
              << (totalIncome - totalCosts) << "\n";
}

/* ============================================================
 *  Reporting
 * ============================================================ */

FleetStats RentalManager::computeStats() const
{
    FleetStats stats;
    const Car *best = nullptr;

    forEachCar([&](Car *car)
               {
                   ++stats.totalCars;
                   switch (car->status())
                   {
                   case CarStatus::Available:
                       ++stats.availableCars;
                       break;
                   case CarStatus::Reserved:
                       ++stats.reservedCars;
                       break;
                   case CarStatus::Rented:
                       ++stats.rentedCars;
                       break;
                   case CarStatus::Maintenance:
                       ++stats.maintenanceCars;
                       break;
                   }
                   stats.activeBookings += car->reservationCount();
                   stats.grossIncome += car->totalIncome();
                   stats.maintenanceSpend += car->maintenanceCost();

                   if (!best || car->totalIncome() > best->totalIncome())
                       best = car;
               });

    stats.netRevenue = stats.grossIncome - stats.maintenanceSpend;
    if (stats.totalCars > 0)
        stats.utilization = 100.0 * static_cast<double>(stats.rentedCars + stats.reservedCars)
                            / static_cast<double>(stats.totalCars);
    if (best && best->totalIncome() > 0.0)
        stats.topEarner = best->name();
    return stats;
}

bool RentalManager::exportFleetReport(const std::string &path) const
{
    const std::filesystem::path parent =
        std::filesystem::path(path).parent_path();
    if (!parent.empty())
        std::filesystem::create_directories(parent);

    std::ofstream file(path);
    if (!file)
        return false;

    // Name-sorted via the AVL in-order walk, so two exports of the
    // same state are byte-identical (diff-friendly).
    file << "name,brand,pricePerDay,status,income,maintenanceCost,netRevenue\n";
    forEachCarSorted([&](Car *car)
                     {
                         file << car->name() << ',' << car->brand() << ','
                              << car->pricePerDay() << ',' << toString(car->status())
                              << ',' << car->totalIncome() << ','
                              << car->maintenanceCost() << ',' << car->netRevenue()
                              << '\n';
                     });
    audit("export.fleet", path);
    return true;
}

bool RentalManager::exportBookingReport(const std::string &path) const
{
    const std::filesystem::path parent =
        std::filesystem::path(path).parent_path();
    if (!parent.empty())
        std::filesystem::create_directories(parent);

    // The per-car heaps expose bookings in bucket order, not service
    // order — flatten, then sort for a stable, diff-friendly report.
    std::vector<const Reservation *> bookings;
    forEachCarSorted([&](Car *car)
                     {
                         car->forEachReservation(
                             [&](const Reservation *r)
                             { bookings.push_back(r); });
                     });
    std::sort(bookings.begin(), bookings.end(),
              [](const Reservation *a, const Reservation *b)
              {
                  if (a->startDate() != b->startDate())
                      return a->startDate() < b->startDate();
                  return a->id() < b->id();
              });

    std::ofstream file(path);
    if (!file)
        return false;

    file << "id,carName,customer,startDate,endDate,durationDays\n";
    for (const Reservation *r : bookings)
        file << r->id() << ',' << r->car()->name() << ','
             << r->customer()->username() << ',' << Date::toISO(r->startDate())
             << ',' << Date::toISO(r->endDate()) << ',' << r->durationDays() << '\n';
    audit("export.bookings", path);
    return true;
}

/* ============================================================
 *  Persistence (CSV round-trip)
 * ============================================================ */

bool RentalManager::hasDataOnDisk(const std::string &dataDir) const
{
    std::ifstream users(dataDir + "/users.csv");
    return users.good();
}

bool RentalManager::saveData(const std::string &dataDir) const
{
    std::filesystem::create_directories(dataDir); // portable mkdir -p

    /* ---- users.csv ---- */
    {
        std::ofstream file(dataDir + "/users.csv");
        if (!file)
            return false;
        file << "username,salt,hash,role,blocked,failedLogins,balance\n";
        userIndex_.forEach([&](const std::string &, User *u)
                           {
                               double balance = (u->role() == "Customer")
                                                    ? static_cast<Customer *>(u)->balance()
                                                    : 0.0;
                               file << u->username() << ',' << u->salt() << ','
                                    << u->passwordHash() << ',' << u->role() << ','
                                    << (u->isBlocked() ? 1 : 0) << ','
                                    << u->failedLogins() << ',' << balance << '\n';
                           });
    }

    /* ---- cars.csv ---- */
    {
        std::ofstream file(dataDir + "/cars.csv");
        if (!file)
            return false;
        file << "name,brand,pricePerDay,status,income,maintenanceCost\n";
        carOrder_.forEach([&](const std::string &name)
                          {
                              Car *const *slot = carIndex_.find(name);
                              if (!slot)
                                  return;
                              Car *c = *slot;
                              file << c->name() << ',' << c->brand() << ','
                                   << c->pricePerDay() << ','
                                   << static_cast<int>(c->status()) << ','
                                   << c->totalIncome() << ','
                                   << c->maintenanceCost() << '\n';
                          });
    }

    /* ---- reservations.csv ---- */
    {
        std::ofstream file(dataDir + "/reservations.csv");
        if (!file)
            return false;
        file << "id,carName,customer,requestTime,startDate,endDate\n";
        carOrder_.forEach([&](const std::string &name)
                          {
                              const Car *const *slot = carIndex_.find(name);
                              if (!slot)
                                  return;
                              (*slot)->forEachReservation([&](const Reservation *r)
                                                          {
                                                              file << r->id() << ',' << name << ','
                                                                   << r->customer()->username() << ','
                                                                   << r->requestTime() << ','
                                                                   << Date::toISO(r->startDate()) << ','
                                                                   << Date::toISO(r->endDate()) << '\n';
                                                          });
                          });
    }

    /* ---- audit_log.csv (append-only trail, v2.5.0) ---- */
    auditLog_.saveCSV(dataDir + "/audit_log.csv");

    return true;
}

bool RentalManager::loadData(const std::string &dataDir)
{
    clearAll();
    auditLog_.loadCSV(dataDir + "/audit_log.csv"); // absent on fresh runs

    /* ---- users first (reservations reference them) ---- */
    {
        std::ifstream file(dataDir + "/users.csv");
        if (!file)
            return false;

        std::string line;
        std::getline(file, line); // header
        while (std::getline(file, line))
        {
            if (line.empty())
                continue;
            std::stringstream ss(line);
            std::string username, salt, hash, role, blockedStr, failedStr, balanceStr;
            std::getline(ss, username, ',');
            std::getline(ss, salt, ',');
            std::getline(ss, hash, ',');
            std::getline(ss, role, ',');
            std::getline(ss, blockedStr, ',');
            std::getline(ss, failedStr, ',');
            std::getline(ss, balanceStr, ',');

            User *user = nullptr;
            if (role == "Customer")
            {
                Customer *c = new Customer(username);
                c->setBalance(std::stod(balanceStr));
                user = c;
            }
            else if (role == "Staff")
                user = new Staff(username);
            else if (role == "Manager")
                user = new Manager(username);
            else
                continue;

            user->restoreCredentials(salt, hash,
                                     blockedStr == "1",
                                     std::stoi(failedStr));
            userIndex_.insert(username, user);
        }
    }

    /* ---- cars ---- */
    {
        std::ifstream file(dataDir + "/cars.csv");
        if (!file)
            return true; // users-only state is still a valid load

        std::string line;
        std::getline(file, line);
        while (std::getline(file, line))
        {
            if (line.empty())
                continue;
            std::stringstream ss(line);
            std::string name, brand, priceStr, statusStr, incomeStr, maintStr;
            std::getline(ss, name, ',');
            std::getline(ss, brand, ',');
            std::getline(ss, priceStr, ',');
            std::getline(ss, statusStr, ',');
            std::getline(ss, incomeStr, ',');
            std::getline(ss, maintStr, ',');

            Car *car = new Car(name, brand, std::stod(priceStr));
            car->setStatus(static_cast<CarStatus>(std::stoi(statusStr)));
            car->addIncome(std::stod(incomeStr));
            car->addMaintenanceCost(std::stod(maintStr));
            carIndex_.insert(name, car);
            carOrder_.pushBack(name);
        }
    }

    /* ---- reservations last (need users + cars) ---- */
    {
        std::ifstream file(dataDir + "/reservations.csv");
        if (!file)
            return true;

        std::string line;
        std::getline(file, line);
        while (std::getline(file, line))
        {
            if (line.empty())
                continue;
            std::stringstream ss(line);
            std::string idStr, carName, customerName, reqStr, startStr, endStr;
            std::getline(ss, idStr, ',');
            std::getline(ss, carName, ',');
            std::getline(ss, customerName, ',');
            std::getline(ss, reqStr, ',');
            std::getline(ss, startStr, ',');
            std::getline(ss, endStr, ',');

            Car **carSlot = carIndex_.find(carName);
            User **userSlot = userIndex_.find(customerName);
            if (!carSlot || !userSlot)
                continue; // dangling booking -> drop it, don't crash

            int id = std::stoi(idStr);
            reservationCounter_ = std::max(reservationCounter_, id);

            // startDate/endDate: ISO dates since 2.3.0, day numbers
            // before that — accept both so old data files keep loading.
            int start = 0, end = 0;
            if (!Date::parseFlexible(startStr, start) ||
                !Date::parseFlexible(endStr, end) || start > end)
                continue; // corrupt row -> drop it, don't crash

            Reservation *r = new Reservation(static_cast<Customer *>(*userSlot),
                                             *carSlot, id, std::stoi(reqStr),
                                             start, end);
            (*carSlot)->reinsertReservation(r);
        }
    }
    return true;
}

void RentalManager::seedDemoData()
{
    // Demo accounts (documented in the README) -------------------
    auto addDemoUser = [&](User *u, const char *pass)
    {
        u->setPassword(pass);
        userIndex_.insert(u->username(), u);
    };
    addDemoUser(new Manager("admin"), "manager123");
    addDemoUser(new Staff("sara"), "staff123");
    addDemoUser(new Customer("ali"), "customer123");

    // Demo fleet ---------------------------------------------------
    struct SeedCar
    {
        const char *name, *brand;
        double price;
    };
    const SeedCar fleet[] = {
        {"Pride 131", "Saipa", 30.0},
        {"Quick R", "Saipa", 35.0},
        {"Peugeot 206", "Peugeot", 45.0},
        {"Peugeot Pars", "Peugeot", 60.0},
        {"Samand LX", "IKCO", 50.0},
        {"Dena Plus", "IKCO", 55.0},
        {"Benz C200", "Mercedes", 180.0},
        {"Tucson 2023", "Hyundai", 120.0}};

    for (const SeedCar &s : fleet)
        addCar(new Car(s.name, s.brand, s.price));
}
