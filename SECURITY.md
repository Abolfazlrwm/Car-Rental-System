# Security Policy

## Supported versions

| Version | Supported |
|---|---|
| 2.x | :white_check_mark: |
| 1.x (course snapshot) | :x: |

## Reporting a vulnerability

Please **do not open a public issue** for anything you believe is a
security problem.

Use GitHub's **Private vulnerability reporting**:
open the repository's **Security** tab → *Report a vulnerability*, and
describe the issue, how to reproduce it, and the impact. You will get a
response within a few days, and a fix will be released with credit if
you want it.

## Scope notes

This is an educational, dependency-free C++17 console project, so the
threat model is modest — but some areas are taken seriously and reports
are very welcome there:

- **Password storage** — salted SHA-256 (`SHA256.{h,cpp}`, `User.cpp`).
  Hashing correctness is regression-tested against official NIST test
  vectors. (Yes, for production systems you would use Argon2/bcrypt —
  see `docs/IMPROVEMENTS.md` for why SHA-256 was the right call *for
  this project*.)
- **CSV round-trip** — malformed or hostile `data/*.csv` files must not
  crash the app or corrupt state (`RentalManager::loadData`).
- **Memory safety** — the full suite is expected to stay clean under
  AddressSanitizer + UBSan; the CI `sanitizers` job enforces this on
  every push.
