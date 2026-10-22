#ifndef SHA256_H
#define SHA256_H

#include <string>

/**
 * ============================================================
 *  SHA256
 * ============================================================
 *  A compact, dependency-free implementation of the SHA-256
 *  message digest (FIPS 180-4), used to store user passwords.
 *
 *  The original project "hashed" passwords with a Caesar
 *  cipher (+3 on every character) — trivially reversible.
 *  This implementation provides real one-way hashing, and each
 *  user additionally gets a random SALT so two users with the
 *  same password produce different digests.
 *
 *  Usage:
 *      std::string digest = SHA256::hash("abc");
 *      // -> "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
 * ============================================================
 */
class SHA256
{
public:
    /// Returns the 64-character lowercase hex digest of `input`.
    static std::string hash(const std::string &input);
};

#endif // SHA256_H
