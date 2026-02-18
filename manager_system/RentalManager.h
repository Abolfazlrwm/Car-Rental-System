#ifndef RENTAL_MANAGER_H
#define RENTAL_MANAGER_H

#include "../basic_structures/DoublyLinkedList.h"
#include "../basic_structures/AVLTree.h"
#include "../basic_structures/HashTable.h"
#include <fstream>
#include <sstream>
#include "Car.h"
#include "User.h"
#include <string>

class RentalManager
{
private:
    DoublyLinkedList userList;
    DoublyLinkedList carList;
    AVLTree carIndex;         
    HashTable userTable;       
    User *currentUser;

    int reservationCounter; // unique reservation IDs

    string hashPassword(const string &pass) const;

public:
    RentalManager();

    // User management
    bool registerUser(const string &username,
                      const string &password,
                      const string &role);

    bool login(const string &username,
               const string &password);

    void blockUser(const string &username);

    void staffDeliverCar(const string &carName);                                          // تبدیل رزرو به امانت
    void staffReturnCar(const string &carName, int returnDate);                           // بازگشت خودرو و جریمه
    void generateIncomeReport();                                                          // گزارش درآمد مدیر
    void recordMaintenance(const string &carName, string date, string desc, double cost); // ثبت تعمیر
    // Car management
    void addCar(Car *car);
    Car *searchCar(const string &name);

    // Reservation
    bool makeReservation(const string &carName,
                         int startDate,
                         int endDate);

    void returnCar(const string &carName);

    void showAllCars();

    void exportCarsToCSV(const string &filename); 
    void importCarsFromCSV(const string &filename);

    void setCarMaintenance(const string &carName);          // برای تعمیرات
    bool renewRental(const string &carName, int extraDays); // برای تمدید
    void payPenalty(const string &username, double amount); // پرداخت جریمه
    // Session info
    bool isLoggedIn() const;
    string getCurrentUserRole() const;
    string getCurrentUsername() const;
    void logout();
};

#endif