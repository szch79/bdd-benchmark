cmake_minimum_required(VERSION 3.15)

project(tbuddy C CXX)

# TBUDDY (Randal E. Bryant, FMCAD 2022), BuDDy extended with the generation of clausal proofs.
# The artifact builds the library with its own Makefile (`src/buddy/Makefile`); this compiles
# the same files into the same archive, `tbuddy.a`, with the same definitions, so that the
# submodule stays unpatched.

set(TBUDDY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../tbuddy-artifact/src/buddy)

add_library(tbuddy STATIC
  ${TBUDDY_DIR}/tbdd.c
  ${TBUDDY_DIR}/prover.c
  ${TBUDDY_DIR}/bddio.c
  ${TBUDDY_DIR}/bvec.c
  ${TBUDDY_DIR}/bddop.c
  ${TBUDDY_DIR}/cache.c
  ${TBUDDY_DIR}/fdd.c
  ${TBUDDY_DIR}/ilist.c
  ${TBUDDY_DIR}/imatrix.c
  ${TBUDDY_DIR}/kernel.c
  ${TBUDDY_DIR}/pairs.c
  ${TBUDDY_DIR}/prime.c
  ${TBUDDY_DIR}/reorder.c
  ${TBUDDY_DIR}/tree.c
  ${TBUDDY_DIR}/cppext.cxx
  ${TBUDDY_DIR}/pseudoboolean.cxx
)

target_include_directories(tbuddy PUBLIC ${TBUDDY_DIR})

# The node layout depends on `ENABLE_TBDD`, so every user of the headers must see it.
target_compile_definitions(tbuddy PUBLIC ENABLE_TBDD=1)

# The artifact always counts cache accesses; here only with statistics, as for BuDDy.
if (BDD_BENCHMARK_STATS)
  target_compile_definitions(tbuddy PRIVATE CACHESTATS)
endif (BDD_BENCHMARK_STATS)

# Optimisation follows the build type, as for the other packages.
target_compile_options(tbuddy PRIVATE -w)

set_target_properties(tbuddy PROPERTIES
  LINKER_LANGUAGE CXX
  C_STANDARD 11
  CXX_STANDARD 11
)
