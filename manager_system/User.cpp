#include "User.h"

#include <iomanip>
#include <random>
#include <sstream>

User::User(const std::string &username, const std::string &role)
    : username_(username), role_(role), blocked_(false), failedLogins_(0) {}

void User::setPassword(const std::string &plainPassword)
{
    // Random 8-byte salt from the OS entropy pool (hex encoded, 16 chars).
    static std::mt19937_64 rng(std::random_device{}());
    static const char *hexDigits = "0123456789abcdef";

    std::ostringstream saltStream;
    for (int i = 0; i < 8; ++i)
    {
        unsigned char byte = static_cast<unsigned char>(rng() & 0xFF);
        saltStream << hexDigits[byte >> 4] << hexDigits[byte & 0x0F];
    }
    salt_ = saltStream.str();

    passwordHash_ = SHA256::hash(salt_ + plainPassword);
}

bool User::verifyPassword(const std::string &plainPassword) const
{
    return SHA256::hash(salt_ + plainPassword) == passwordHash_;
}

std::string Customer::serializeExtra() const
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << balance_;
    return oss.str();
}
