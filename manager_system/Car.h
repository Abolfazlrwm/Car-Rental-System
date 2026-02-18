#ifndef CAR_H
#define CAR_H

#include <string>
#include <iostream>
#include "../basic_structures/PQ.h"
#include "Reservation.h"

using namespace std;

enum CarStatus
{
    AVAILABLE,
    RESERVED,
    RENTED,
    MAINTENANCE
};

// استراکچر برای ثبت تاریخچه تعمیرات
struct MaintenanceRecord
{
    string date;
    string description;
    double cost;
    MaintenanceRecord *next;

    MaintenanceRecord(string d, string desc, double c)
        : date(d), description(desc), cost(c), next(nullptr) {}
};

class Car
{
private:
    string name;
    string brand;
    double pricePerDay;
    CarStatus status;
    double totalIncome; 

    PriorityQueue reservationQueue;
    MaintenanceRecord *maintenanceHistory; // لیست تعمیرات

public:
    Car(const string &name, const string &brand, double pricePerDay);

    // Getters
    string getName() const;
    string getBrand() const;
    double getPrice() const;
    CarStatus getStatus() const;
    double getIncome() const;

    // Status control
    void setStatus(CarStatus s);
    void addIncome(double amount);
    void addMaintenanceLog(const string &date, const string &desc, double cost);
    void showMaintenanceHistory() const;

    // Reservation logic
    void reserve(Reservation *r);
    Reservation *getCurrentReservation() const;
    void returnCar();
    bool hasReservations() const;
    PriorityQueue &getQueue() { return reservationQueue; }
};

#endif