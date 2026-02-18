#ifndef RESERVATION_H
#define RESERVATION_H

#include <string>

class Customer;
class Car;

class Reservation
{
private:
    Customer *customer;
    Car *car;

    int requestTime; // timestamp when reservation is made
    int startDate;   // rental start date
    int endDate;

public:
    Reservation(Customer *customer, Car *car,
                int requestTime, int startDate, int endDate);

    int getRequestTime() const;
    int getStartDate() const;
    int getEndDate() const;
    Customer *getCustomer() const;
    Car *getCar() const;

    // comparison for Priority Queue
    bool hasHigherPriorityThan(const Reservation *other) const;
};

#endif