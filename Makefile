# ------------------------------------------------------------------
#  Convenience Makefile (CMake is the primary build system)
#
#    make          build the app + tests
#    make test     run the unit tests
#    make run      build & start the app
#    make bench    build & run the container benchmarks
#    make asan     build & run the tests under AddressSanitizer + UBSan
#    make wasm     compile the app to WebAssembly for the browser demo
#    make format   reformat all sources with clang-format
#    make clean    remove build artifacts
# ------------------------------------------------------------------

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
SRC       = mainApp.cpp manager_system/SHA256.cpp manager_system/User.cpp \
            manager_system/Car.cpp manager_system/Reservation.cpp \
            manager_system/Date.cpp manager_system/RentalManager.cpp \
            manager_system/JsonExport.cpp manager_system/Analytics.cpp \
            manager_system/AuditLog.cpp manager_system/WaitlistEntry.cpp \
            manager_system/RentalHistory.cpp manager_system/MonthlyReport.cpp
TEST_SRC  = tests/test_main.cpp manager_system/SHA256.cpp manager_system/User.cpp \
            manager_system/Car.cpp manager_system/Reservation.cpp \
            manager_system/Date.cpp manager_system/RentalManager.cpp \
            manager_system/JsonExport.cpp manager_system/Analytics.cpp \
            manager_system/AuditLog.cpp manager_system/WaitlistEntry.cpp \
            manager_system/RentalHistory.cpp manager_system/MonthlyReport.cpp
BENCH_SRC = benchmarks/bench_containers.cpp

all: car_rental unit_tests

car_rental: $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $@

unit_tests: $(TEST_SRC)
	$(CXX) $(CXXFLAGS) $(TEST_SRC) -o $@

test: unit_tests
	./unit_tests

run: car_rental
	./car_rental

# Container benchmarks (hand-written vs the standard library).
bench: $(BENCH_SRC)
	$(CXX) -std=c++17 -Wall -Wextra -O2 $(BENCH_SRC) -o bench_containers
	./bench_containers

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

# Run the REAL binary in the browser (v3.3.0): Emscripten compiles the
# same sources to WebAssembly and lands the artifacts in docs-site/demo/,
# which GitHub Pages serves at /demo.html. The demo page speaks to the
# program through docs-site/demo/pre.js (Asyncify-backed stdin). Build
# with emsdk 3.1.x on PATH as `emcc` — https://emscripten.org
wasm: docs-site/demo/car_rental.js

docs-site/demo/car_rental.js: $(SRC) docs-site/demo/pre.js
	@command -v emcc >/dev/null 2>&1 || \
            { echo "emcc is not installed — https://emscripten.org"; exit 1; }
	emcc -std=c++17 -O2 $(SRC) \
            -o docs-site/demo/car_rental.js \
            -sASYNCIFY -sFORCE_FILESYSTEM -sALLOW_MEMORY_GROWTH=1 \
            -sINVOKE_RUN=0 -sEXIT_RUNTIME=0 \
            -sEXPORTED_RUNTIME_METHODS=callMain,HEAPU8 \
            -sENVIRONMENT=web \
            --pre-js docs-site/demo/pre.js

clean:
	rm -f car_rental unit_tests unit_tests_asan bench_containers

.PHONY: all test run bench asan wasm format clean
