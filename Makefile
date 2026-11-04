# ------------------------------------------------------------------
#  Convenience Makefile (CMake is the primary build system)
#
#    make          build the app + tests
#    make test     run the unit tests
#    make run      build & start the app
#    make bench    build & run the container benchmarks
#    make asan     build & run the tests under AddressSanitizer + UBSan
#    make format   reformat all sources with clang-format
#    make clean    remove build artifacts
# ------------------------------------------------------------------

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
SRC       = mainApp.cpp manager_system/SHA256.cpp manager_system/User.cpp \
            manager_system/Car.cpp manager_system/Reservation.cpp \
            manager_system/Date.cpp manager_system/RentalManager.cpp
TEST_SRC  = tests/test_main.cpp manager_system/SHA256.cpp manager_system/User.cpp \
            manager_system/Car.cpp manager_system/Reservation.cpp \
            manager_system/Date.cpp manager_system/RentalManager.cpp

all: car_rental unit_tests

car_rental: $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $@

unit_tests: $(TEST_SRC)
	$(CXX) $(CXXFLAGS) $(TEST_SRC) -o $@

test: unit_tests
	./unit_tests

run: car_rental
	./car_rental

# Full suite under AddressSanitizer + UBSan -- same job CI runs on every push.
asan:
	$(CXX) -std=c++17 -g -Wall -Wextra -fsanitize=address,undefined \
	    $(TEST_SRC) -o unit_tests_asan
	./unit_tests_asan

# House style lives in .clang-format; requires clang-format 14+ on PATH.
format:
	@command -v clang-format >/dev/null 2>&1 || \
	    { echo "clang-format is not installed"; exit 1; }
	clang-format -i mainApp.cpp manager_system/*.cpp tests/*.cpp basic_structures/*.h

clean:
	rm -f car_rental unit_tests unit_tests_asan

.PHONY: all test run asan format clean
