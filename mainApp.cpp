#include <iostream>
#include <string>
#include "manager_system/RentalManager.h"

using namespace std;

/* ---------- Menus ---------- */

void showGuestMenu()
{
    cout << "\n=== Welcome ===\n";
    cout << "1. Register\n";
    cout << "2. Login\n";
    cout << "3. Exit\n";
    cout << "Choice: ";
}

void showCustomerMenu()
{
    cout << "\n=== Customer Menu ===\n";
    cout << "1. View cars\n";
    cout << "2. Make reservation\n";
    cout << "3. Renew Rental\n";
    cout << "4. Logout\n";
    cout << "Choice: ";
}

void showStaffMenu()
{
    cout << "\n=== Staff Menu ===\n";
    cout << "1. View cars\n";
    cout << "2. Return car\n";
    cout << "3. Set car to Maintenance\n";
    cout << "4. deliver car\n";
    cout << "5. Logout\n";
    cout << "Choice: ";
}

void showManagerMenu()
{
    cout << "\n=== Manager Menu ===\n";
    cout << "1. Add car\n";
    cout << "2. View cars\n";
    cout << "3. Block user\n";
    cout << "4. Export data\n";
    cout << "5. Import data\n";
    cout << "6. Logout\n";
    cout << "Choice: ";
}

/* ---------- Main ---------- */

int main()
{
    RentalManager system;
    bool running = true;

    while (running)
    {
        if (!system.isLoggedIn())
        {
            showGuestMenu();
            int choice;
            cin >> choice;

            if (choice == 1)
            {
                string user, pass, role;
                cout << "Username: ";
                cin >> user;
                cout << "Password: ";
                cin >> pass;
                cout << "Role (Customer / Staff / Manager): ";
                cin >> role;

                if (system.registerUser(user, pass, role))
                    cout << "Registered successfully.\n";
                else
                    cout << "User already exists.\n";
            }
            else if (choice == 2)
            {
                string user, pass;
                cout << "Username: ";
                cin >> user;
                cout << "Password: ";
                cin >> pass;

                if (system.login(user, pass))
                    cout << "Login successful.\n";
                else
                    cout << "Login failed.\n";
            }
            else if (choice == 3)
            {
                running = false;
            }
        }
        else
        {
            string role = system.getCurrentUserRole();

            /* ---------- CUSTOMER ---------- */
            if (role == "Customer")
            {
                showCustomerMenu();
                int choice;
                cin >> choice;

                if (choice == 1)
                {
                    system.showAllCars();
                }
                else if (choice == 2)
                {
                    string carName;
                    int start, end;

                    cout << "Car name: ";
                    cin >> carName;
                    cout << "Start date: ";
                    cin >> start;
                    cout << "End date: ";
                    cin >> end;

                    if (system.makeReservation(carName, start, end))
                        cout << "Reservation successful.\n";
                    else
                        cout << "Reservation failed.\n";
                }
                else if (choice == 3)
                {
                    string name;
                    int days;
                    cout << "Car name: ";
                    cin >> name;
                    cout << "Extra days: ";
                    cin >> days;
                    system.renewRental(name, days);
                }
                else if (choice == 4)
                {
                    system.logout();
                }
            }

            /* ---------- STAFF ---------- */
            else if (role == "Staff")
            {
                showStaffMenu();
                int choice;
                cin >> choice;

                if (choice == 1)
                {
                    system.showAllCars();
                }
                else if (choice == 2)
                {
                    // بازگشت خودرو
                    string name;
                    int date;
                    cout << "Car name: ";
                    cin >> name;
                    cout << "Current Date (int): ";
                    cin >> date;
                    system.staffReturnCar(name, date);
                }
                else if (choice == 3)
                {
                    // تعمیرات
                    string name, date, desc;
                    double cost;
                    cout << "Car name: ";
                    cin >> name;
                    cout << "Date: ";
                    cin >> date;
                    cout << "Description: ";
                    cin >> desc;
                    cout << "Cost: ";
                    cin >> cost;
                    system.recordMaintenance(name, date, desc, cost);
                }
                else if (choice == 4)
                { //  تحویل خودرو
                    string name;
                    cout << "Car name to deliver to customer: ";
                    cin >> name;
                    system.staffDeliverCar(name);
                }
                else if (choice == 5)
                { // Logout was 4, moved to 5
                    system.logout();
                }
            }

            /* ---------- MANAGER ---------- */
            else if (role == "Manager")
            {
                showManagerMenu();
                int choice;
                cin >> choice;

                if (choice == 1)
                {
                    string name, brand;
                    double price;
                    cout << "Name: ";
                    cin >> name;
                    cout << "Brand: ";
                    cin >> brand;
                    cout << "Price: ";
                    cin >> price;
                    Car *newCar = new Car(name, brand, price);
                    system.addCar(newCar);
                    cout << "Car added.\n";
                }
                else if (choice == 2)
                { // View Cars + Income
                    system.generateIncomeReport();
                }
                else if (choice == 3)
                {
                    string user;
                    cout << "Username to block: ";
                    cin >> user;
                    system.blockUser(user);
                    cout << "User blocked.\n";
                }
                else if (choice == 4)
                {
                    system.exportCarsToCSV("cars_backup.csv");
                    cout << "Exported.\n";
                }
                else if (choice == 5)
                {
                    system.importCarsFromCSV("cars_backup.csv");
                    cout << "Imported.\n";
                }
                else if (choice == 6)
                    system.logout();
            }
        }
    }
    return 0;
}