#ifndef USER_H
#define USER_H

#include <string>
using namespace std;

class User
{
protected:
    string username;
    string passwordHash;
    string role;
    bool blocked;

public:
    User(const string &username, const string &passwordHash, const string &role);
    virtual ~User() {}
    string getUsername() const;
    string getPasswordHash() const;
    string getRole() const;
    bool isBlocked() const;
    void block();
};

// تعریف کلاس‌های فرزند برای جلوگیری از ارور incomplete type
class Customer : public User
{
public:
    Customer(const string &u, const string &p, const string &r) : User(u, p, r) {}
};

class Staff : public User
{
public:
    Staff(const string &u, const string &p, const string &r) : User(u, p, r) {}
};

class Manager : public User
{
public:
    Manager(const string &u, const string &p, const string &r) : User(u, p, r) {}
};

#endif