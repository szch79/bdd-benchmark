# The Aytdd package of BDDs with complement edges, consumed through its C interface.
#
# Aytdd is a Lean library, so it is not built here.  Build the archive in the Aytdd checkout
# with `lake build libaytdd`, and the archive over the open table, for AYTDD_OPEN_TABLE, with
# `lake -R -KnodeTable=openTable build libaytdd`, after which `lake -R build` returns the checkout
# to its default build.  When this harness is the submodule benchmark/BDD/bdd-benchmark
# of that checkout, AYTDD_ROOT defaults to it; otherwise point AYTDD_ROOT at the checkout.  The
# archive carries the whole Lean closure and the glue; the link file beside it names the
# remaining archives and the linker flags of the Lean runtime, one per line.  The headers of the
# counts include gmp.h, which the Lean toolchain does not ship, so they take its directory from the
# GMP that the top-level file finds through pkg-config.

set(AYTDD_ROOT "" CACHE PATH "Path to the Aytdd checkout providing libaytdd.a")

if (NOT AYTDD_ROOT AND EXISTS "${CMAKE_SOURCE_DIR}/../../../lakefile.lean")
  get_filename_component(AYTDD_ROOT "${CMAKE_SOURCE_DIR}/../../.." ABSOLUTE)
endif()

if (NOT AYTDD_ROOT)
  message(FATAL_ERROR "AYTDD is ON but AYTDD_ROOT is not set")
endif()

set(AYTDD_INCLUDE "${AYTDD_ROOT}/capi/include")

# Defines the imported target TARGET for the archive that the command COMMAND writes into the
# build directory BUILD_DIR of the checkout, together with its link file.
function(aytdd_import TARGET BUILD_DIR COMMAND)
  set(ARCHIVE "${AYTDD_ROOT}/${BUILD_DIR}/lib/libaytdd.a")
  set(LINKFILE "${AYTDD_ROOT}/${BUILD_DIR}/lib/libaytdd.link")
  foreach (FILE ${ARCHIVE} ${LINKFILE})
    if (NOT EXISTS "${FILE}")
      message(FATAL_ERROR "no ${FILE}; run `${COMMAND}` in ${AYTDD_ROOT}")
    endif()
  endforeach()

  file(STRINGS "${LINKFILE}" LINK_FLAGS)

  add_library(${TARGET} STATIC IMPORTED GLOBAL)
  set_target_properties(${TARGET} PROPERTIES
    IMPORTED_LOCATION "${ARCHIVE}"
    INTERFACE_INCLUDE_DIRECTORIES "${AYTDD_INCLUDE};${GMP_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES "${LINK_FLAGS}"
  )

  message(STATUS "  |  Aytdd archive:     ${ARCHIVE}")
endfunction()

# The default build runs over the parent trie, and the build of the option AYTDD_OPEN_TABLE over
# the open table, in a build directory of its own.
if (AYTDD)
  aytdd_import(aytdd .lake/build "lake build libaytdd")
endif()
if (AYTDD_OPEN_TABLE)
  aytdd_import(aytdd_ot .lake/build-openTable "lake -R -KnodeTable=openTable build libaytdd")
endif()
