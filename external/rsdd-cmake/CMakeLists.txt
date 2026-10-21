cmake_minimum_required(VERSION 3.15)

project(rsdd-bindings)

# RSDD (Steven Holtzen et al.) is a Rust library whose C interface is part of the crate itself, in
# its module `src/ffi` behind the feature `ffi`. This builds the crate of the submodule
# `external/rsdd` as a static library with that feature and generates the header with cbindgen
# from the crate's own `cbindgen.toml`, so that the submodule stays unpatched.

set(RSDD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../rsdd)

# --- Settings -----------------------------------------------------------------

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# This could be derived from the $<CONFIG> if desired.
set(RSDD_CARGO_PROFILE "release" CACHE STRING "Sets the build profile used for Rust dependencies (`dev`, `release`, or `release-with-debug`)")
set_property(CACHE RSDD_CARGO_PROFILE PROPERTY STRINGS "dev" "release" "release-with-debug")

# Mark some cmake variables as advanced since they don't have to be exposed to the user.
mark_as_advanced(FORCE
    CORROSION_BUILD_TESTS
    CORROSION_DEV_MODE
    CORROSION_NATIVE_TOOLING
    CORROSION_NO_WARN_PARSE_TARGET_TRIPLE_FAILED
    CORROSION_RESPECT_OUTPUT_DIRECTORY
    CORROSION_VERBOSE_OUTPUT
    Rust_RESOLVE_RUSTUP_TOOLCHAINS)


# --- Build Dependencies -------------------------------------------------------

# Use FetchContent to acquire Corrosion for the cmake integration of Rust projects.
include(FetchContent)

FetchContent_Declare(
    Corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    GIT_TAG v0.5.2
)
FetchContent_MakeAvailable(Corrosion)

find_program(CBINDGEN_EXECUTABLE
    NAMES cbindgen
    DOC "Requires cbindgen to generate the RSDD header"
    REQUIRED)

# The crate has no lock file in its repository, which ignores `Cargo.lock`. The one next to this
# file pins the dependencies; it is copied into the crate, where Git ignores it, and Cargo runs
# with `--locked` against it.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Cargo.lock ${RSDD_DIR}/Cargo.lock COPYONLY)


# --- Main Targets -------------------------------------------------------------

# Generate rsdd.h using cbindgen. The crate's configuration leaves the condition `feature = "ffi"`
# of the interface without a `[defines]` entry, which cbindgen reports once per item and which
# does not change the header, so cbindgen runs quietly.
set(rsdd_include ${PROJECT_BINARY_DIR}/include)
set(rsdd_h ${rsdd_include}/rsdd.h)
file(GLOB_RECURSE rsdd_sources CONFIGURE_DEPENDS ${RSDD_DIR}/src/*.rs)
add_custom_command(
    OUTPUT ${rsdd_h}
    COMMAND ${CBINDGEN_EXECUTABLE}
    ARGS --quiet --output ${rsdd_h} ${RSDD_DIR}
    DEPENDS ${rsdd_sources} ${RSDD_DIR}/cbindgen.toml ${RSDD_DIR}/Cargo.toml
    COMMENT "Generating rsdd.h using cbindgen"
    VERBATIM)
# Custom target such that targets in other directories can depend on the header
add_custom_target(rsdd-header ALL DEPENDS ${rsdd_h})

# Import the library of the crate as a static library with its C interface
corrosion_import_crate(
    MANIFEST_PATH ${RSDD_DIR}/Cargo.toml
    PROFILE ${RSDD_CARGO_PROFILE}
    CRATES rsdd
    CRATE_TYPES staticlib
    FEATURES ffi
    LOCKED)
add_dependencies(rsdd rsdd-header)
# The generated header declares an empty structure, which C++ compilers warn about in user headers
target_include_directories(rsdd SYSTEM INTERFACE ${rsdd_include})
