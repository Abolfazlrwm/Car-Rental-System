#include "RentalManager.h"
#include <iostream>

RentalManager::RentalManager()
{
    currentUser = nullptr;
    reservationCounter = 0;
}

// ---------------- Utility ----------------
string RentalManager::hashPassword(const string &pass) const
{
    string hashed = pass;
    for (int i = 0; i < hashed.length(); i++)
        hashed[i] += 3;
    return hashed;
}

// ---------------- User Management ----------------
bool RentalManager::registerUser(const string &username, const string &password, const string &role)
{
    if (userTable.find(username) != nullptr)
        return false;

    string hashedPass = hashPassword(password);

    User *newUser = nullptr;
    if (role == "Customer")
        newUser = new Customer(username, hashedPass, role);
    else if (role == "Staff")
        newUser = new Staff(username, hashedPass, role);
    else
        newUser = new Manager(username, hashedPass, role);

    userTable.insert(username, newUser);
    userList.pushBack(username);
    return true;
}

bool RentalManager::login(const string &username, const string &password)
{
    User *u = userTable.find(username);
    if (u->getUsername() != username)
        return false;
    if (u->isBlocked())
        return false;
    if (u->getPasswordHash() != hashPassword(password))
        return false;
    currentUser = u;
    return true;
}

void RentalManager::blockUser(const string &username)
{
    User *u = userTable.find(username);
    if (u)
        u->block();
}

// ---------------- Car Management ----------------
void RentalManager::addCar(Car *car)
{
    carList.pushBack(car->getName());
    carIndex.insert(car->getName(), car);
}

Car *RentalManager::searchCar(const string &name)
{
    return carIndex.find(name);
}

// ---------------- Reservation ----------------
bool RentalManager::makeReservation(const string &carName, int startDate, int endDate)
{
    if (!currentUser)
        return false;

    Car *car = carIndex.find(carName);
    if (!car || car->getStatus() == MAINTENANCE)
    {
        cout << "Car is not available or under maintenance.\n";
        return false;
    }

    // ۱. چک کردن همپوشانی تاریخ (Double-booking Prevention)
    if (car->getQueue().hasOverlap(startDate, endDate))
    {
        cout << "Error: This car is already reserved for the selected dates!\n";
        return false;
    }

    Customer *cust = (Customer *)currentUser;
    Reservation *r = new Reservation(cust, car, reservationCounter++, startDate, endDate);

    car->reserve(r);
    cout << "Reservation confirmed for " << carName << ".\n";
    car->addIncome(car->getPrice());
    return true;
}

void RentalManager::returnCar(const string &carName)
{
    Car *car = carIndex.find(carName);
    if (!car)
        return;
    car->returnCar();
}

// ---------------- Display & Reports ----------------
void RentalManager::showAllCars()
{
    if (carList.Size() == 0)
    {
        cout << "--- No cars found in the system! ---\n";
        return;
    }
    cout << "\n--- List of All Cars ---\n";
    for (int i = 0; i < carList.Size(); i++)
    {
        string name = carList.getAt(i);
        Car *car = carIndex.find(name);
        if (car)
        {
            cout << "Name: " << car->getName()
                 << " | Brand: " << car->getBrand()
                 << " | Price: " << car->getPrice()
                 << " | Status: " << car->getStatus()
                 << endl;
        }
    }
}

void RentalManager::exportCarsToCSV(const string &filename)
{
    ofstream file(filename);
    if (!file.is_open())
        return;
    file << "Name,Brand,Price,Status\n";
    for (int i = 0; i < carList.Size(); i++)
    {
        string name = carList.getAt(i);
        Car *car = carIndex.find(name);
        if (!car)
            continue;
        file << car->getName() << "," << car->getBrand() << ","
             << car->getPrice() << "," << car->getStatus() << "\n";
    }
    file.close();
}

void RentalManager::importCarsFromCSV(const string &filename)
{
    ifstream file(filename);
    if (!file.is_open())
        return;
    string line;
    getline(file, line);
    while (getline(file, line))
    {
        string name, brand, priceStr, statusStr;
        stringstream ss(line);
        getline(ss, name, ',');
        getline(ss, brand, ',');
        getline(ss, priceStr, ',');
        getline(ss, statusStr, ',');
        double price = stod(priceStr);
        int status = stoi(statusStr);
        Car *car = new Car(name, brand, price);
        car->setStatus((CarStatus)status);
        carList.pushBack(name);
        carIndex.insert(name, car);
    }
    file.close();
}

// ---------------- Other Features ----------------
bool RentalManager::isLoggedIn() const { return currentUser != nullptr; }
string RentalManager::getCurrentUserRole() const { return currentUser ? currentUser->getRole() : ""; }
string RentalManager::getCurrentUsername() const { return currentUser ? currentUser->getUsername() : ""; }
void RentalManager::logout() { currentUser = nullptr; }

void RentalManager::setCarMaintenance(const string &carName)
{
    Car *car = searchCar(carName);
    if (car)
    {
        if (car->getStatus() != RENTED)
        {
            car->setStatus(MAINTENANCE);
            cout << "Car " << carName << " is now under maintenance.\n";
        }
        else
            cout << "Cannot maintain a rented car!\n";
    }
    else
        cout << "Car not found.\n";
}

bool RentalManager::renewRental(const string &carName, int extraDays)
{
    Car *car = searchCar(carName);
    if (!car || car->getStatus() != RENTED)
    {
        cout << "Car is not currently rented.\n";
        return false;
    }
    if (car->hasReservations())
    {
        cout << "Cannot renew: Other reservations are waiting.\n";
        return false;
    }
    cout << "Rental extended for " << extraDays << " days.\n";
    return true;
}

// ---------------- Staff & Manager Logic ----------------
void RentalManager::staffDeliverCar(const string &carName)
{
    Car *car = carIndex.find(carName);
    if (!car)
    {
        cout << "Car not found.\n";
        return;
    }

    if (car->getStatus() == RESERVED && car->hasReservations())
    {
        car->setStatus(RENTED);
        cout << "Car delivered to customer: " << car->getCurrentReservation()->getCustomer()->getUsername() << endl;
    }
    else
        cout << "Car is not reserved or queue is empty.\n";
}

void RentalManager::staffReturnCar(const string &carName, int actualReturnDate)
{
    Car *car = carIndex.find(carName);
    if (!car || car->getStatus() != RENTED)
    {
        cout << "Car is not currently rented.\n";
        return;
    }

    Reservation *res = car->getCurrentReservation();
    int scheduledEndDate = res->getEndDate();
    double dailyPrice = car->getPrice();
    double totalCost = 0;

    // ۱. محاسبه هزینه پایه (مدت زمان رزرو شده)
    int duration = scheduledEndDate - res->getStartDate() + 1;
    totalCost = duration * dailyPrice;

    // ۲. محاسبه جریمه دیرکرد (Penalty)
    if (actualReturnDate > scheduledEndDate)
    {
        int delayDays = actualReturnDate - scheduledEndDate;
        double penaltyRate = 1.5; // نرخ جریمه (قابل تغییر)
        double penalty = delayDays * (dailyPrice * penaltyRate);
        totalCost += penalty;
        cout << "Delay detected: " << delayDays << " days. Penalty: " << penalty << endl;
    }

    car->addIncome(totalCost);
    car->returnCar(); // وضعیت خودرو را AVAILABLE یا RESERVED می‌کند

    cout << "Car returned successfully. Total amount charged: " << totalCost << endl;
}

void RentalManager::recordMaintenance(const string &carName, string date, string desc, double cost)
{
    Car *car = carIndex.find(carName);
    if (car)
    {
        car->addMaintenanceLog(date, desc, cost);
        setCarMaintenance(carName);
    }
}

void RentalManager::generateIncomeReport()
{
    double total = 0;
    cout << "\n--- Income Report ---\n";
    for (int i = 0; i < carList.Size(); i++)
    {
        Car *car = carIndex.find(carList.getAt(i));
        if (car)
        {
            cout << car->getName() << ": " << car->getIncome() << endl;
            total += car->getIncome();
        }
    }
    cout << "Total Revenue: " << total << endl;
}