# The Aytdd package of BDDs with complement edges, consumed through its C interface.
#
# Aytdd is a Lean library, so it is not built here.  Build the archive in the Aytdd checkout
# with `lake build libaytdd`.  When this harness is the submodule benchmark/BDD/bdd-benchmark
# of that checkout, AYTDD_ROOT defaults to it; otherwise point AYTDD_ROOT at the checkout.  The
# archive carries the whole Lean closure and the glue; the link file beside it names the
# remaining archives and the linker flags of the Lean runtime, one per line.

set(AYTDD_ROOT "" CACHE PATH "Path to the Aytdd checkout providing libaytdd.a")

if (NOT AYTDD_ROOT AND EXISTS "${CMAKE_SOURCE_DIR}/../../../lakefile.lean")
  get_filename_component(AYTDD_ROOT "${CMAKE_SOURCE_DIR}/../../.." ABSOLUTE)
endif()

if (NOT AYTDD_ROOT)
  message(FATAL_ERROR "AYTDD is ON but AYTDD_ROOT is not set")
endif()

set(AYTDD_ARCHIVE "${AYTDD_ROOT}/.lake/build/lib/libaytdd.a")
set(AYTDD_LINKFILE "${AYTDD_ROOT}/.lake/build/lib/libaytdd.link")
set(AYTDD_INCLUDE "${AYTDD_ROOT}/capi/include")

if (NOT EXISTS "${AYTDD_ARCHIVE}")
  message(FATAL_ERROR "no ${AYTDD_ARCHIVE}; run `lake build libaytdd` in ${AYTDD_ROOT}")
endif()
if (NOT EXISTS "${AYTDD_LINKFILE}")
  message(FATAL_ERROR "no ${AYTDD_LINKFILE}; run `lake build libaytdd` in ${AYTDD_ROOT}")
endif()

file(STRINGS "${AYTDD_LINKFILE}" AYTDD_LINK_FLAGS)

if (APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

add_library(aytdd STATIC IMPORTED GLOBAL)
set_target_properties(aytdd PROPERTIES
  IMPORTED_LOCATION "${AYTDD_ARCHIVE}"
  INTERFACE_INCLUDE_DIRECTORIES "${AYTDD_INCLUDE}"
  INTERFACE_LINK_LIBRARIES "${AYTDD_LINK_FLAGS}"
)

message(STATUS "  |  Aytdd archive:     ${AYTDD_ARCHIVE}")
