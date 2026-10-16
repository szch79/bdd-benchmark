cmake_minimum_required(VERSION 3.15)

project (bdd_benchmarks
  VERSION 1
  DESCRIPTION "A collection of comparable BDD Benchmarks"
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Default build type: Release" FORCE)
endif()

# ============================================================================ #
# Compiler settings
# ============================================================================ #
message(STATUS "")
message(STATUS "CMake build configuration for BDD Benchmarks")
message(STATUS "  OS:                   ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Compiler:             ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  |  C Flags:           ${CMAKE_C_FLAGS}")
message(STATUS "  |  CXX Flags:         ${CMAKE_CXX_FLAGS}")
message(STATUS "  |  EXE Linker Flags:  ${CMAKE_EXE_LINKER_FLAGS}")

message(STATUS "  Options:")

option(BDD_BENCHMARK_GRENDEL "Build for Grendel cluster" OFF)
message(STATUS "  |  Grendel:           ${BDD_BENCHMARK_GRENDEL}")

option(BDD_BENCHMARK_INCL_INIT "Include initialisation time in total" OFF)
message(STATUS "  |  Incl. Init:        ${BDD_BENCHMARK_INCL_INIT}")

option(BDD_BENCHMARK_STATS "Build with printing of verbose statistics (ruins time measurements)" OFF)
message(STATUS "  |  Statistics:        ${BDD_BENCHMARK_STATS}")
if (BDD_BENCHMARK_STATS)
  message(WARN " | NOTE: THIS MAKES ANY REPORTED TIME MEASUREMENTS NOT REPRESENTATIVE!")
endif(BDD_BENCHMARK_STATS)

option(BDD_BENCHMARK_WAIT "Wait to exit program until enter has been pressed." OFF)
message(STATUS "  |  Waiting:           ${BDD_BENCHMARK_WAIT}")

message(STATUS "")

# ============================================================================ #
# External
# ============================================================================ #

# ---------------------------------------------------------------------------- #
# BDD Packages

# Adiar Package (Steffan Sølvsten - Aarhus University, Denmark)
option(ADIAR "Build for Adiar" ON)
if (ADIAR)
  if (BDD_BENCHMARK_STATS)
    set(ADIAR_STATS ON)
  endif(BDD_BENCHMARK_STATS)

  add_subdirectory (external/adiar adiar EXCLUDE_FROM_ALL)
endif(ADIAR)

# Aytdd Package (Mingtong Lin), a Lean library used through its C interface
option(AYTDD "Build for Aytdd" ON)
option(AYTDD_OPEN_TABLE "Build for Aytdd over its open table" OFF)
if (AYTDD OR AYTDD_OPEN_TABLE)
  add_subdirectory(external/aytdd aytdd EXCLUDE_FROM_ALL)
endif (AYTDD OR AYTDD_OPEN_TABLE)

# BuDDy Package (Jørn Lind-Nielsen - Copenhagen University, Denmark)
option(BUDDY "Build for BuDDy" ON)
if (BUDDY)
  set(BUDDY_EXAMPLES OFF)
  if (BDD_BENCHMARK_STATS)
    set(BUDDY_STATS ON)
  endif(BDD_BENCHMARK_STATS)

  add_subdirectory (external/buddy buddy EXCLUDE_FROM_ALL)
endif (BUDDY)

# Cal Package (Jagesh Sanghavi, Rajeev Ranjan et al. - University of California, United States of America)
option(CAL "Build for CAL" ON)
if (CAL)
  add_subdirectory (external/cal cal EXCLUDE_FROM_ALL)
endif (CAL)

# CUDD Package (Fabio Somenzi - University of Colorado Boulder, United States of America)
option(CUDD "Build for CUDD" ON)
if (CUDD)
  add_subdirectory(external/cudd cudd EXCLUDE_FROM_ALL)
endif (CUDD)

# Biodivine/LibBDD (Samuel Pastva et al. - Institute of Science and Technology Austria)
option(LIBBDD "Build for LibBDD" ON)
if (LIBBDD)
  add_subdirectory(external/lib-bdd-ffi lib-bdd-ffi EXCLUDE_FROM_ALL)
endif (LIBBDD)

# OxiDD (Nils Husung et al. - Saarland University, Germany and Eindhoven University of Technology, The Netherlands)
option(OXIDD "Build for OxiDD" ON)
if (OXIDD)
  add_subdirectory(external/oxidd oxidd EXCLUDE_FROM_ALL)
endif (OXIDD)

# Ruddy (Lukáš Urban, Samuel Pastva - Masaryk University, Czech Republic), through a C interface
option(RUDDY "Build for Ruddy" ON)
if (RUDDY)
  add_subdirectory(external/ruddy-ffi ruddy-ffi EXCLUDE_FROM_ALL)
endif (RUDDY)

# SimpleABC (Alan Mishchenko, Yukio Miyasaka - University of California, United States of America and University of Tokyo, Japan)
option(SIMPLEABC "Build for SimpleABC" ON)
if (SIMPLEABC)
  # ABC requires a CMake version older than 3.5, which CMake 4 only accepts with this variable.
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
  set(READLINE_FOUND FALSE)
  add_subdirectory(external/abc-bdd abc-bdd EXCLUDE_FROM_ALL)
  unset(CMAKE_POLICY_VERSION_MINIMUM)
  unset(READLINE_FOUND)
endif (SIMPLEABC)

# Sylvan Package (Tom van Dijk - University of Twente, Netherlands)
option(SYLVAN "Build for Sylvan (Saarland & Eindhoven Univesity, Germany & Netherlands)" ON)
if (SYLVAN)
  if (BDD_BENCHMARK_STATS)
    set(SYLVAN_STATS ON)
  endif(BDD_BENCHMARK_STATS)

  add_subdirectory (external/sylvan sylvan EXCLUDE_FROM_ALL)
endif (SYLVAN)

# TBUDDY (Randal E. Bryant - Carnegie Mellon University, United States of America), BuDDy
# generating clausal proofs, built for the CNF benchmark only
option(TBUDDY "Build for TBUDDY" ON)
if (TBUDDY)
  add_subdirectory(external/tbuddy tbuddy EXCLUDE_FROM_ALL)
endif (TBUDDY)

# ---------------------------------------------------------------------------- #
# BLIF parser
add_subdirectory (external/blifparse libblifparse EXCLUDE_FROM_ALL)

# Boost, which only the mcnet benchmarks need
option(MCNET "Build the mcnet benchmarks, which require Boost" ON)
if (MCNET)
  find_package(Boost REQUIRED)
endif (MCNET)

# XML parser
add_subdirectory (external/pugixml pugixml EXCLUDE_FROM_ALL)

# ============================================================================ #
# Benchmarks
# ============================================================================ #
add_subdirectory (src)

# ============================================================================ #
# Formatting
# ============================================================================ #
include(cmake/clang-cxx-dev-tools.cmake)
