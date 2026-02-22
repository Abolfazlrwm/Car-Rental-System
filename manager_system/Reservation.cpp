#include "Reservation.h"

Reservation::Reservation(Customer *customer, Car *car,
                         int requestTime, int startDate, int endDate)
    : customer(customer),
      car(car),
      requestTime(requestTime),
      startDate(startDate),
      endDate(endDate) {}

int Reservation::getRequestTime() const
{
    return requestTime;
}

int Reservation::getStartDate() const
{
    return startDate;
}

int Reservation::getEndDate() const {
    return endDate;
}

Customer *Reservation::getCustomer() const
{
    return customer;
}

Car *Reservation::getCar() const
{
    return car;
}

bool Reservation::hasHigherPriorityThan(const Reservation *other) const
{
    // اولویت اول: تاریخ شروع کمتر (زودتر)
    if (this->startDate != other->startDate)
        return this->startDate < other->startDate;
    // اولویت دوم: اگر تاریخ شروع یکی بود، هر کس زودتر درخواست داده (FIFO)
    return this->requestTime < other->requestTime;
}