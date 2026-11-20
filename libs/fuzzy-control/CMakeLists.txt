# fuzzy-control : pilotage direct des controleurs (Maschine MK3 pour commencer)
# Licence : GPLv3. Protocole MK3 repris du pilote "Maschine Zynthian Prototype" r22yb3m (MIT).
cmake_minimum_required(VERSION 3.22)
project(FuzzyControl VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)   # MacPorts : sudo port install libusb

# Decodage des rapports (sans materiel : testable partout)
add_library(fuzzy_control
    src/decoder.cpp
    src/lights.cpp
    src/maschine_mk3.cpp)
target_include_directories(fuzzy_control PUBLIC include)
target_link_libraries(fuzzy_control PUBLIC PkgConfig::LIBUSB)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(fuzzy_control PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Outil de test sur le materiel : affiche tout ce que fait la Maschine
add_executable(fuzzy-control-probe tools/probe.cpp)
target_link_libraries(fuzzy-control-probe PRIVATE fuzzy_control)

# Tests automatiques du decodage (pas besoin de Maschine)
enable_testing()
add_executable(fuzzy-control-tests tests/decoder_tests.cpp)
target_link_libraries(fuzzy-control-tests PRIVATE fuzzy_control)
add_test(NAME decoder COMMAND fuzzy-control-tests)
