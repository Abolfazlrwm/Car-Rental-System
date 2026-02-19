#include "User.h"

User::User(const string &username,
           const string &passwordHash,
           const string &role)
    : username(username),
      passwordHash(passwordHash),
      role(role),
      blocked(false) {}

string User::getUsername() const
{
    return username;
}

string User::getPasswordHash() const
{
    return passwordHash;
}

string User::getRole() const
{
    return role;
}

bool User::isBlocked() const
{
    return blocked;
}

void User::block()
{
    blocked = true;
}