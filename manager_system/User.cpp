#include "User.h"

#include <iomanip>
#include <random>
#include <sstream>

User::User(const std::string &username, const std::string &role)
    : username_(username), role_(role), blocked_(false), failedLogins_(0)
{
}

namespace
{

// PBKDF2 iteration count. OWASP's 2023 guidance for PBKDF2-HMAC-SHA256 is
// 600k; for this CLI demo 100k keeps a login attempt well under a second on
// commodity hardware while still making offline guessing 100,000x more
// expensive than the single SHA-256 pass it replaces. Tune upward for
// anything user-facing at scale.
constexpr int kPbkdf2Iterations = 100000;

/// Random 16-byte salt drawn directly from the OS CSPRNG (std::random_device
/// is the only standard-library entropy source that is allowed to be and,
/// on every supported platform, is a real CSPRNG; seeding a deterministic
/// generator like mt19937_64 from it, as before, buys nothing).
std::string randomSaltHex()
{
    static const char *hexDigits = "0123456789abcdef";
    std::random_device device;
    std::ostringstream saltStream;
    for (int i = 0; i < 16; ++i)
    {
        const auto byte = static_cast<unsigned char>(device());
        saltStream << hexDigits[byte >> 4] << hexDigits[byte & 0x0F];
    }
    return saltStream.str();
}

/// Decodes a hex digest string into its raw bytes.
std::string hexDecode(const std::string &hex)
{
    auto nibble = [](char c) -> unsigned char
    {
        if (c >= '0' && c <= '9') return static_cast<unsigned char>(c - '0');
        return static_cast<unsigned char>(c - 'a' + 10);
    };
    std::string raw;
    raw.reserve(hex.size() / 2);
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
        raw += static_cast<char>((nibble(hex[i]) << 4) | nibble(hex[i + 1]));
    return raw;
}

/// HMAC-SHA256 (RFC 2104) built on the repo's own one-shot SHA-256:
/// H((K xor opad) || H((K xor ipad) || m)). SHA256::hash() speaks hex, so the
/// inner digest is decoded back to its raw 32 bytes before it feeds the outer
/// hash — the construction is over raw bytes, never over hex characters.
std::string hmacSHA256(const std::string &key, const std::string &message)
{
    constexpr std::size_t blockSize = 64; // SHA-256 block size in bytes

    std::string k = key;
    if (k.size() > blockSize)
        k = hexDecode(SHA256::hash(k));
    k.resize(blockSize, '\x00');

    std::string inner, outer;
    inner.reserve(2 * blockSize + message.size());
    outer.reserve(2 * blockSize);
    for (std::size_t i = 0; i < blockSize; ++i)
    {
        inner += static_cast<char>(k[i] ^ 0x36);
        outer += static_cast<char>(k[i] ^ 0x5c);
    }
    return SHA256::hash(outer + hexDecode(SHA256::hash(inner + message)));
}

/// PBKDF2-HMAC-SHA256 (RFC 2898) for a single 32-byte block, hex encoded.
/// The XOR chain runs on the raw 32-byte digests, never on their hex forms.
std::string pbkdf2SHA256(const std::string &salt, const std::string &password)
{
    // U1 = HMAC(P, S || INT(1)) with the big-endian 1 as its last byte.
    std::string u = hexDecode(hmacSHA256(password, salt + std::string("\x00\x00\x00\x01", 4)));
    std::string accumulator = u;
    for (int i = 1; i < kPbkdf2Iterations; ++i)
    {
        u = hexDecode(hmacSHA256(password, u));
        for (std::size_t j = 0; j < accumulator.size(); ++j)
            accumulator[j] = static_cast<char>(accumulator[j] ^ u[j]);
    }

    static const char *hexDigits = "0123456789abcdef";
    std::ostringstream out;
    for (unsigned char byte : accumulator)
        out << hexDigits[byte >> 4] << hexDigits[byte & 0x0F];
    return out.str();
}

/// Byte-wise comparison that always walks the full strings, so the number of
/// operations does not depend on where the first differing byte sits.
bool constantTimeEquals(const std::string &a, const std::string &b)
{
    // Compare against both lengths; a mismatch in length still walks a full
    // pass so timing does not reveal how much of the digest matched.
    const unsigned char *x = reinterpret_cast<const unsigned char *>(a.data());
    const unsigned char *y = reinterpret_cast<const unsigned char *>(b.data());
    const std::size_t n = a.size() > b.size() ? a.size() : b.size();
    unsigned char diff = static_cast<unsigned char>(a.size() ^ b.size());
    for (std::size_t i = 0; i < n; ++i)
        diff |= static_cast<unsigned char>(x[i] ^ y[i]);
    return diff == 0;
}

} // namespace

void User::setPassword(const std::string &plainPassword)
{
    salt_ = randomSaltHex();
    passwordHash_ = deriveHash(salt_, plainPassword);
}

std::string User::deriveHash(const std::string &salt, const std::string &plainPassword)
{
    return pbkdf2SHA256(salt, plainPassword);
}

bool User::verifyPassword(const std::string &plainPassword) const
{
    return constantTimeEquals(deriveHash(salt_, plainPassword), passwordHash_);
}

std::string Customer::serializeExtra() const
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << balance_;
    return oss.str();
}
