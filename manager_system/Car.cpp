#include "Car.h"
#include <iostream>

using namespace std;

Car::Car(const string &name, const string &brand, double pricePerDay)
    : name(name),
      brand(brand),
      pricePerDay(pricePerDay),
      status(AVAILABLE), // الان AVAILABLE را می‌شناسد
      totalIncome(0),
      reservationQueue(50),
      maintenanceHistory(nullptr)
{
}

string Car::getName() const { return name; }
string Car::getBrand() const { return brand; }
double Car::getPrice() const { return pricePerDay; }
CarStatus Car::getStatus() const { return status; }
double Car::getIncome() const { return totalIncome; }

void Car::setStatus(CarStatus s) { status = s; }

void Car::addIncome(double amount) { totalIncome += amount; }

void Car::addMaintenanceLog(const string &date, const string &desc, double cost)
{
    MaintenanceRecord *newRecord = new MaintenanceRecord(date, desc, cost);
    newRecord->next = maintenanceHistory;
    maintenanceHistory = newRecord;
}

void Car::showMaintenanceHistory() const
{
    MaintenanceRecord *curr = maintenanceHistory;
    cout << "Maintenance History for " << name << ":\n";
    while (curr)
    {
        cout << "Date: " << curr->date << " | Desc: " << curr->description << " | Cost: " << curr->cost << endl;
        curr = curr->next;
    }
}

void Car::reserve(Reservation *r)
{
    reservationQueue.insert(r);
    if (status == AVAILABLE)
        status = RESERVED;
}

Reservation *Car::getCurrentReservation() const
{
    return reservationQueue.peek();
}

void Car::returnCar()
{
    if (!reservationQueue.isEmpty())
        reservationQueue.extractMin();

    if (reservationQueue.isEmpty())
        status = AVAILABLE;
    else
        status = RESERVED;
}

bool Car::hasReservations() const
{
    return !reservationQueue.isEmpty();
}