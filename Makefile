# ------------------------------------------------------------------
#  Convenience Makefile (CMake is the primary build system)
#
#    make          build the app + tests
#    make test     run the unit tests
#    make run      build & start the app
#    make clean    remove build artifacts
# ------------------------------------------------------------------

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
SRC       = mainApp.cpp manager_system/SHA256.cpp manager_system/User.cpp \
            manager_system/Car.cpp manager_system/Reservation.cpp \
            manager_system/RentalManager.cpp
TEST_SRC  = tests/test_main.cpp manager_system/SHA256.cpp manager_system/User.cpp \
            manager_system/Car.cpp manager_system/Reservation.cpp \
            manager_system/RentalManager.cpp

all: car_rental unit_tests

car_rental: $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $@

unit_tests: $(TEST_SRC)
	$(CXX) $(CXXFLAGS) $(TEST_SRC) -o $@

test: unit_tests
	./unit_tests

run: car_rental
	./car_rental

clean:
	rm -f car_rental unit_tests

.PHONY: all test run clean
