# Contributing to Car Rental System

Thanks for your interest in improving the project! This repo is a
dependency-free C++17 codebase, so everything you need is a compiler.

## Getting started

```bash
git clone git@github.com:Abolfazlrwm/Car-Rental-System.git
cd Car-Rental-System
make            # builds car_rental + unit_tests
make test       # 2,100+ checks must pass before anything else
```

CMake works too: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j`.

## Ground rules

- **C++17, zero external dependencies.** The hand-written containers in
  `basic_structures/` are the point of the project — do not replace them
  with `std::` equivalents.
- **Every bug fix ships with a regression test** in `tests/test_main.cpp`.
  The suite has caught real v1.0 bugs (segfaults, income double-counting,
  fake renewals) and should keep doing so.
- `RentalManager` is the facade for the business layer; keep console I/O
  inside `mainApp.cpp` only.
- Match the existing style (4-space indent, header guards as
  `#pragma once`, comments explain *why* not *what*).

## Commit messages

We follow [Conventional Commits](https://www.conventionalcommits.org/):
`feat:`, `fix:`, `refactor:`, `test:`, `docs:`, `build:`, `ci:`, `chore:`.

## Pull requests

- One topic per PR; keep the diff reviewable.
- CI must be green (GCC + Clang, CMake build, ctest, raw-compile pass).
- If you change user-visible behavior, update the README and
  `docs/IMPROVEMENTS.md`.
