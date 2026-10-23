# The package Aytdd of BDDs with complement edges, consumed through its C interface.
#
# Aytdd is a Lean library whose archives lake builds in the Aytdd checkout.  This file runs that
# build for each build of Aytdd that the options name and imports the archive it writes.  The
# default build runs `lake -R build libaytdd`, and the build over the open table, for
# AYTDD_OPEN_TABLE, runs `lake -R -KnodeTable=openTable build libaytdd`.  Lake keeps the options of
# a configuration until the next one, so the builds under options run first and the default build
# runs last, which returns the checkout to its default configuration.  The lake that runs them is
# the lake on the path, unless the cache variable LAKE names another.  When this harness is the submodule
# benchmark/BDD/bdd-benchmark of that checkout, AYTDD_ROOT defaults to it; otherwise point
# AYTDD_ROOT at the checkout.  The archive carries the whole Lean closure and the glue; the link
# file beside it names the remaining archives and the linker flags of the Lean runtime, one per
# line.  The headers of the counts include gmp.h, which the Lean toolchain does not ship, so they
# take its directory from the GMP that the top-level file finds through pkg-config.

set(AYTDD_ROOT "" CACHE PATH "Path to the Aytdd checkout providing libaytdd.a")

if (NOT AYTDD_ROOT AND EXISTS "${CMAKE_SOURCE_DIR}/../../../lakefile.lean")
  get_filename_component(AYTDD_ROOT "${CMAKE_SOURCE_DIR}/../../.." ABSOLUTE)
endif()

if (NOT AYTDD_ROOT)
  message(FATAL_ERROR "AYTDD is ON but AYTDD_ROOT is not set")
endif()

set(AYTDD_INCLUDE "${AYTDD_ROOT}/capi/include")

set(LAKE lake CACHE FILEPATH "The lake that builds the archives of Aytdd")

# Builds the archive of Aytdd with lake under the options that follow BUILD_DIR, which write it into
# the build directory BUILD_DIR of the checkout together with its link file.
function(aytdd_build BUILD_DIR)
  set(COMMAND ${LAKE} ${ARGN} build libaytdd)
  list(JOIN COMMAND " " COMMAND_LINE)
  message(STATUS "  |  Running ${COMMAND_LINE} in ${AYTDD_ROOT}")
  execute_process(
    COMMAND ${COMMAND}
    WORKING_DIRECTORY "${AYTDD_ROOT}"
    RESULT_VARIABLE STATUS
  )
  if (NOT STATUS EQUAL 0)
    message(FATAL_ERROR "`${COMMAND_LINE}` in ${AYTDD_ROOT} failed with ${STATUS}")
  endif()

  foreach (FILE lib/libaytdd.a lib/libaytdd.link)
    if (NOT EXISTS "${AYTDD_ROOT}/${BUILD_DIR}/${FILE}")
      message(FATAL_ERROR "no ${AYTDD_ROOT}/${BUILD_DIR}/${FILE} after `${COMMAND_LINE}`")
    endif()
  endforeach()
endfunction()

# Builds the archive of Aytdd as aytdd_build does and defines the imported target TARGET for it.
function(aytdd_import TARGET BUILD_DIR)
  aytdd_build(${BUILD_DIR} ${ARGN})

  set(ARCHIVE "${AYTDD_ROOT}/${BUILD_DIR}/lib/libaytdd.a")
  file(STRINGS "${AYTDD_ROOT}/${BUILD_DIR}/lib/libaytdd.link" LINK_FLAGS)

  add_library(${TARGET} STATIC IMPORTED GLOBAL)
  set_target_properties(${TARGET} PROPERTIES
    IMPORTED_LOCATION "${ARCHIVE}"
    INTERFACE_INCLUDE_DIRECTORIES "${AYTDD_INCLUDE};${GMP_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES "${LINK_FLAGS}"
  )

  message(STATUS "  |  Aytdd archive:     ${ARCHIVE}")
endfunction()

# The default build runs over the parent trie, and the build of the option AYTDD_OPEN_TABLE over
# the open table, in a build directory of its own.  The builds of the options AYTDD_T0 to AYTDD_T3
# run over the parent trie at the trust levels T0 to T3, each in a build directory of its own.  The
# default build runs after them all, also when AYTDD is off, so that the checkout returns to its
# default configuration.
if (AYTDD_OPEN_TABLE)
  aytdd_import(aytdd_ot .lake/build-openTable -R -KnodeTable=openTable)
endif()
foreach (LEVEL T0 T1 T2 T3)
  if (AYTDD_${LEVEL})
    string(TOLOWER ${LEVEL} NAME)
    aytdd_import(aytdd_${NAME} .lake/build-${LEVEL} -R -Ktrust=${LEVEL})
  endif()
endforeach()
if (AYTDD)
  aytdd_import(aytdd .lake/build -R)
else()
  aytdd_build(.lake/build -R)
endif()
