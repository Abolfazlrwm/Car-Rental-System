cmake_minimum_required(VERSION 3.10)
project(CarRentalSystem
        VERSION 3.8.0
        DESCRIPTION "Car Rental Management System — modern C++17 with hand-built data structures"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warnings: keep the codebase honest.
if(MSVC)
    add_compile_options(/W4 /utf-8)  # Persian UI literals are UTF-8
else()
    add_compile_options(-Wall -Wextra)
endif()

# ---------------------------------------------------------------- app
add_executable(car_rental
    mainApp.cpp
    manager_system/SHA256.cpp
    manager_system/User.cpp
    manager_system/Car.cpp
    manager_system/Reservation.cpp
    manager_system/Date.cpp
    manager_system/I18n.cpp
    manager_system/Png.cpp
    manager_system/RestServer.cpp
    manager_system/RentalManager.cpp
    manager_system/JsonExport.cpp
    manager_system/Analytics.cpp
    manager_system/AuditLog.cpp
    manager_system/WaitlistEntry.cpp
    manager_system/RentalHistory.cpp
    manager_system/MonthlyReport.cpp)
target_include_directories(car_rental PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# The REST server uses Winsock on Windows.
if(WIN32)
    target_link_libraries(car_rental PRIVATE ws2_32)
endif()

# ---------------------------------------------------------------- tests
enable_testing()
add_executable(unit_tests
    tests/test_main.cpp
    manager_system/SHA256.cpp
    manager_system/User.cpp
    manager_system/Car.cpp
    manager_system/Reservation.cpp
    manager_system/Date.cpp
    manager_system/I18n.cpp
    manager_system/Png.cpp
    manager_system/RestServer.cpp
    manager_system/RentalManager.cpp
    manager_system/JsonExport.cpp
    manager_system/Analytics.cpp
    manager_system/AuditLog.cpp
    manager_system/WaitlistEntry.cpp
    manager_system/RentalHistory.cpp
    manager_system/MonthlyReport.cpp)
target_include_directories(unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
    target_link_libraries(unit_tests PRIVATE ws2_32)
endif()
add_test(NAME unit_tests COMMAND unit_tests)

# ---------------------------------------------------------------- benchmarks
# Built by `cmake --build` but never run automatically — benchmarks are
# for humans (`make bench`), not for CI gating.
add_executable(bench_containers benchmarks/bench_containers.cpp)
target_include_directories(bench_containers PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
