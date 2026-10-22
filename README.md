# BDD Benchmarking Suite

This is a collection of benchmarks for Binary Decision Diagrams (BDDs)
[[Bryant1986](#references)] and some of its variants. These are useful to survey
the strengths and weaknesses of different implementations and to guide their
developers. To this end, each benchmark is implemented by use of C++ templates
such that they are agnostic of the BDD package used.

This project has been developed at the
[Logic and Semantics](https://logsem.github.io/) group at
[Aarhus University](https://cs.au.dk).

<!-- markdown-toc start - Don't edit this section. Run M-x markdown-toc-refresh-toc -->
**Table of Contents**

- [Decision Diagrams](#decision-diagrams)
- [BDD Packages](#bdd-packages)
    - [Dependencies](#dependencies)
- [Usage](#usage)
    - [Build](#build)
    - [Run](#run)
- [Benchmarks](#benchmarks)
    - [Apply](#apply)
    - [CNF Solver](#cnf-solver)
    - [Game of Life](#game-of-life)
    - [Hamiltonian Cycle](#hamiltonian-cycle)
    - [McNet](#mcnet)
    - [Picotrav](#picotrav)
    - [QBF Solver](#qbf-solver)
    - [Queens](#queens)
    - [RelProd](#relprod)
    - [Tic-Tac-Toe](#tic-tac-toe)
- [Performance Regression Testing](#performance-regression-testing)
- [License](#license)
- [Citation](#citation)
- [Publications](#publications)
- [References](#references)

<!-- markdown-toc end -->

## Decision Diagrams

Our benchmarks target the following types of decision diagrams.

- [**Binary Decision Diagrams**](https://en.wikipedia.org/wiki/Binary_decision_diagram)
  **(BDDs)**
  [[Bryant1986](#references)]

- [**Binary Decision Diagrams with Complemented Edges**](https://en.wikipedia.org/wiki/Binary_decision_diagram#Complemented_edges)
  **(BCDDs)**
  [[Brace1990](#references)]

- [**Zero-suppressed Decision Diagrams**](https://en.wikipedia.org/wiki/Zero-suppressed_decision_diagram)
  **(ZDDs)**
  [[Minato1993](#references)]

## BDD Packages

The benchmarks are implemented using C++ templates to make them agnostic of the
BDD package used. To do so, each implementation has an *adapter* in-between
which is fully inlined at compile time. Currently, we support the following BDD
packages.

- [**Adiar**](https://github.com/ssoelvsten/adiar)
  [[Sølvsten2022](#references)]:

  An I/O-efficient implementation based on [[Arge1995](#references)] with
  iterative algorithms that use time-forward processing
  [[Arge1996](#references), [Chiang1995](#references)] to exploit a special
  sorting of BDD nodes streamed from and to the disk. These algorithms have no
  need for memoization or garbage collection. But, on the other hand, nodes are
  also not shareable between BDDs.


- **Aytdd**:

  An implementation of shared BDDs with complement edges in Lean 4, with
  depth-first algorithms using a unique node table and memoization. Nodes are
  never reclaimed. It counts weighted models exactly, over the rationals, and in
  double precision.

  We use its C interface, which is built in the Aytdd repository.


- [**BuDDy**](https://buddy.sourceforge.net/manual/main.html)
  [[Lind-Nielsen1999](#references)]:

  An easy-to-use yet extensive implementation of shared BDDs with depth-first
  algorithms using a unique node table and memoization.

  We use [this](https://github.com/SSoelvsten/BuDDy) version that has been
  updated and builds using CMake.


- [**CAL**](https://github.com/SSoelvsten/cal)
  [[Sanghavi1996](#references)]:

  A breadth-first implementation based on [[Ochi1993](#references),
  [Ashar1994](#references)] that exploits a specific level-by-level locality of
  nodes on disk to improve performance when dealing with large BDDs. Unlike
  Adiar it also supports sharing of nodes between BDDs at the cost of
  memoization and garbage collection.

  We use the [revived version](https://github.com/SSoelvsten/cal) with an
  extended C API, CMake support and a C++ API.


- **CUDD**
  [[Somenzi2015](#references)]:

  Probably the most popular BDD package of all. It uses depth-first algorithms
  and a unique node table and memoization.

  We use [this modified v3.0](https://github.com/SSoelvsten/cudd) with CMake
  support and an extended C++ API.


- [**LibBDD**](https://github.com/sybila/biodivine-lib-bdd)
  [[Beneš2020](#references)]:

  A thread-safe implementation with depth-first algorithms and memoization. Yet
  unlike others, it does not implement *shared* BDDs, i.e., two diagrams do not
  share common subgraphs. Hence, the unique and memoization tables are neither
  reused between operations.

  We use [this unofficial Rust-to-C FFI](https://github.com/nhusung/lib-bdd-ffi).


- [**OxiDD**](https://github.com/OxiDD/oxidd)
  [[Husung2024](#references)]:

  A multi-threaded (and thread-safe) framework for the implementation of
  decision diagrams and their algorithms. Currently, its algorithms are
  depth-first on a unique node table and memoization.


- [**RSDD**](https://github.com/neuppl/rsdd):

  A library of binary and sentential decision diagrams. Its BDDs have complement
  edges and depth-first algorithms over a unique node table, without garbage
  collection, as in Aytdd. Its memory differs from Aytdd's in its cache and in
  its nodes. The apply cache that its C interface exposes is a hash map that
  keeps the result of every if-then-else computed, in an entry of 64 bytes. A
  node takes 112 bytes, of which its variable and its two children take 40 and
  a scratch field for traversals and a cached semantic hash the other 72
  (`BddNode` in `src/repr/bdd.rs` of the crate), and it is found through a slot
  of 24 bytes in the hash-consing table, which doubles once it is 70 percent
  full (`src/backing_store/bump_table.rs`). Once the table has outgrown its
  initial 131,072 slots, a node thus takes between 146 and 181 bytes, against
  roughly 24 bytes for a node of Aytdd. The interface has no quantification, so
  the adapter quantifies and renames one variable at a time through its
  single-variable composition. It has no count of the allocated nodes, which
  *Picotrav* prints as `null`, and no export of BDDs. RSDD counts weighted
  models in `f64`. It bounds no table, so `-M` has no effect.

  We use the C interface in its crate, built with the feature `ffi`.


- [**Ruddy**](https://github.com/sybila/ruddy)
  [[Pastva2023](#references)]:

  A library with iterative algorithms whose unique node table is a trie local to
  each parent node rather than a hash table. It has no computed table that
  survives an operation, and no complement edges. Only its shared representation
  is benchmarked. It has no renaming, so the adapter renames a variable by
  quantifying it from the conjunction with its biconditional to the new one.

  We use the C FFI in `external/ruddy-ffi`, written for these benchmarks.


- [**SimpleABC**](https://github.com/MyskYko/abc-bdd)
  [[Miyasaka2019](#references)]:

  The simple BDD package of a fork of ABC, with depth-first algorithms, a unique
  node table and a direct-mapped cache of conjunctions. It has no reference
  counts; the adapter drives its garbage collection from the roots the
  benchmark still holds. Its quantification has no memoization, and the adapter
  calls it as it is. It has no model count, the only package without one, so the
  adapter counts models as CUDD does, by the fraction of the satisfying
  assignments.


- [**Sylvan**](https://github.com/trolando/sylvan)
  [[Dijk2016](#references)]:

  A multi-threaded (and thread-safe) implementation with depth-first algorithms
  using a unique node table and memoization.


- [**TBUDDY**](https://github.com/rebryant/tbuddy-artifact)
  [[Bryant2022](#references)]:

  BuDDy extended with the generation of clausal proofs. Every BDD it computes
  carries a clause of an LRAT proof stating that the input clauses imply it, so
  that deriving false refutes them. It is built only for the CNF benchmark, see
  its option `-p`. TBUDDY chooses the initial size of its node table itself and
  grows the table up to the nodes that fit into `-M`.


|                    | **Adiar** | **Aytdd** | **BuDDy** | **CAL** | **CUDD** | **LibBDD** | **OxiDD** | **RSDD** | **Ruddy** | **SimpleABC** | **Sylvan** | **TBUDDY** |
|--------------------|-----------|-----------|-----------|---------|----------|------------|-----------|----------|-----------|---------------|------------|------------|
| **Language**       | C++       | Lean      | C         | C       | C        | Rust       | Rust      | Rust     | Rust      | C             | C          | C          |
|                    |           |           |           |         |          |            |           |          |           |               |            |            |
| **BDD**            | ✓         |           | ✓         |         | (✓)      | ✓          | ✓         |          | ✓         |               | (✓)        | ✓          |
| **BCDD**           |           | ✓         |           | ✓       | ✓        |            | ✓         | ✓        |           | ✓             | ✓          |            |
| **MTBDD**          |           |           |           |         | ✓        |            | ✓         |          |           |               | ✓          |            |
| **ZDD**            | ✓         |           |           |         | ✓        |            | ✓         |          |           |               | ✓          |            |
|                    |           |           |           |         |          |            |           |          |           |               |            |            |
| **Thread Safe**    |           |           |           |         |          | ✓          | ✓         |          |           |               | ✓          |            |
| **Multi-threaded** |           |           |           |         |          |            | ✓         |          |           |               | ✓          |            |
|                    |           |           |           |         |          |            |           |          |           |               |            |            |
| **Reordering**     |           |           | ✓         | ✓       | ✓        |            | (✓)       |          |           | (✓)           | (✓)        |            |
|                    |           |           |           |         |          |            |           |          |           |               |            |            |
| **Shared Nodes**   |           | ✓         | ✓         | ✓       | ✓        |            | ✓         | ✓        | ✓         | ✓             | ✓          | ✓          |
| **Ext. Memory**    | ✓         |           |           | ✓       |          |            |           |          |           |               |            |            |


We hope to extend the number of packages. See [issue
#12](https://github.com/SSoelvsten/bdd-benchmark/issues/12) for a list of BDD
packages we would like to have added to this set of benchmarks. Any help to do
so is very much appreciated.


### Dependencies
All packages use CMake to build. This makes compilation and linking very simple.
One merely has to initialize all submodules (recursively) using the following
command.

```
git submodule update --init --recursive
```

To build, one needs *CMake* and a *C++* compiler of your choice. On Ubuntu, one
can obtain these with the following command:

```bash
apt install cmake g++
```

The *Picotrav* benchmark requires GNU Bison and Flex. On Ubuntu, these can be
installed with.

```bash
apt install bison flex
```

The *CNF* benchmark requires the *GMP Library*, which is found through
*pkg-config*. On Ubuntu, both can be installed with

```bash
apt install libgmp-dev pkg-config
```

The *McNet* benchmark requires the *Boost Library*. On Ubuntu, these can be
installed as follows

```bash
apt install libboost-all-dev
```

**Adiar**

Adiar also has a transitive dependency on the *Boost Library*. On Ubuntu, these
can be installed as follows

```bash
apt install libboost-all-dev
```

**Aytdd**

Aytdd is a Lean library, which this CMake project does not build, so the package
is off by default and switched on with `-D AYTDD=ON`. Build its archive with
`lake build libaytdd` in the Aytdd repository. When this repository is its
submodule, the archive is found automatically; otherwise, pass its path with
`-D AYTDD_ROOT=<path>`. Its
headers include *gmp.h* of the *GMP Library*, found as for the CNF benchmark.
The archive carries the GMP of the Lean toolchain, so the Aytdd executables
link that one rather than the GMP that *pkg-config* finds.

The default build of Aytdd stores its nodes in parent-local tries. A second
build over its open-addressing node table is the package `aytdd_ot`, switched on
with `-D AYTDD_OPEN_TABLE=ON`. Its archive is built with
`lake -R -KnodeTable=openTable build libaytdd`, into a build directory of its
own, and `lake -R build` returns the repository to its default build.

**LibBDD**, **OxiDD**, **RSDD** and **Ruddy**

These libraries are implemented in Rust and interact with C/C++ via an FFI.
Hence, one needs to use *cargo* which in turn requires an internet connection to
download the dependency crates during the first build ("crate" is the Rust term
for a package).

There are different ways to install Rust. On all platforms, it is possible to
use the [official installer script](https://www.rust-lang.org/tools/install) for
*rustup*, the toolchain manager. Some OSes also provide a *rustup* or
*rustup-init* package. If you have *rustup* installed, then you can install the
latest stable Rust toolchain (including *cargo*) using `rustup default stable`.
Some OSes also provide a fairly recent Rust package. On Ubuntu, you can simply
install *cargo* via:

```bash
apt install cargo
```

Furthermore, the libraries depend on *cbindgen* to generate a C header from Rust
code. *cbindgen* can either be installed through Cargo or your OS package
manager. Note however, that a relatively recent version of *cbindgen* is
required (we tested 0.26.0, which is *not* provided by Ubuntu versions prior to
24.04).

```bash
cargo install --force cbindgen
```

> [!NOTE]
> If installing *cbindgen* through cargo, remember to update your *PATH*.
> Otherwise, CMake will abort with an "*Could not find toolchain ''*" error. You
> can obtain the path from the print message during *cargo install* which states
> "*warning: be sure to add <THE_PATH> to your PATH ...*"

**SimpleABC**

The build of ABC extracts its list of source files by running GNU Make at
configure time, so *make* must be installed even when CMake generates for
another build tool. The whole of ABC is compiled once; only the archive members
the benchmarks use are linked.

**TBUDDY**

The library is compiled from the unmodified artifact in
`external/tbuddy-artifact` by `external/tbuddy/CMakeLists.txt`, from the files
that the artifact's own Makefile puts into `tbuddy.a` and with its definition
`ENABLE_TBDD`.


## Usage

When all dependencies have been resolved (see [above](#dependencies)), you can
build and run the benchmarks as follows.

### Build

To configure the CMake project run:
```bash
cmake -B build
```

The default settings can be changed by also parsing various parameters to CMake.
Here are the values that might be relevant.

- **`-D CMAKE_BUILD_TYPE=<Release|Debug|RelWithDebInfo|...>`** (default: *Release*)

  Change the build type. Except for development purposes, leaving it at *Release*
  is propaly the most correct choice.

- **`-D CMAKE_C_COMPILER=<...>`, `-D CMAKE_CXX_COMPILER=<...>`**

  Select a specific C/C++ compiler.

- **`-G <Generator>`**

  Select the build generator; on most systems, `Makefile` would be the default.
  To speed up the build process you may want to try to use `Ninja` instead.


Features of the benchmarks and BDD packages can also be turned on and off as follows

- **`-D BDD_BENCHMARK_INCL_INIT=<OFF|ON>`** (default: *OFF*)

  If `ON`, includes the initialization time of the BDD Package in the total running
  time.

- **`-D BDD_BENCHMARK_STATS=<OFF|ON>`** (default: *OFF*)

  If `ON`, build with statistics. This might affect performance.

- **`-D BDD_BENCHMARK_WAIT=<OFF|ON>`** (default: *OFF*)

  If `ON`, pause before deinitialising the BDD package and exiting. This can be
  used to investigate its state with external tools, e.g., use of Hugepages.

- **`-D MCNET=<OFF|ON>`** (default: *ON*)

  If `OFF`, the *McNet* benchmark is not built. It is the only benchmark that
  needs the *Boost Library*, which Adiar needs as well, so that a build without
  Boost also needs `-D ADIAR=OFF`.

After configuring, you can build the benchmarks:
```bash
cmake --build build
```

### Run

After building, the *build/src* folder contains one executable for every
possible combination of BDD library, benchmark, and DD kind. Not all libraries
support every kind of DD (see [above](#bdd-packages)) and not all benchmarks
are available for BDDs/BCDDs or ZDDs.

The executables follows a `<Library>_<Benchmark>_<DD>` naming scheme (each of
the three components in *nocase*). All executables have a common command line
interface to configure the BDD library in question:

- **`-h`**

  Print a help text with all options (including the ones particular to that
  particular benchmark).

- **`-M <int>`** (default: *128*)

  Amount of memory (in MiB) to be dedicated to the BDD library, see
  [Memory](#memory).

  Note, that the memory you set is only for the BDD package. So, the program
  will either use swap or be killed if the BDD package takes up more memory in
  conjunction with the benchmark's auxiliary data structures.

- **`-P <int>`** (default: *1*)

  (Maximum) worker thread count for multi-threaded BDD libraries, e.g., OxiDD
  and Sylvan.

- **`-R`**

  Enable dynamic variable reordering (if available with said BDD package). The
  CNF benchmark rejects it together with `-s` and `-b`, which read the variable
  order as fixed.

- **`-T <path>`** (default: */tmp*, */usr/tmp/*, ...)

  Filepath for temporary files on disk for external memory libraries, e.g.,
  Adiar.

For example, you can run the Queens benchmark on Sylvan with 1024 MiB
of memory and 4 threads as follows:
```bash
./build/src/sylvan_queens_bcdd -M 1024 -P 4
```

Furthermore, each benchmark requires options. See `-h` or the [Benchmarks
Section](#benchmarks) for details.

### Memory

The option `-M` is the memory of the BDD package for its nodes and its caches.
Each adapter passes it to the package's own limit where the package has one, and
otherwise derives from it the sizes that the package lets it choose, so that
what `-M` bounds, and how strictly, is a property of the package. The
executables do not bound the memory of a whole run, which includes the data of
the benchmark; a cap on it, the same for every run, is left to whatever runs
them. A package that the adapter's code finds out of memory ends the run with
status 4, which the benchmarks reserve for this outcome, as they reserve status
3 for a task that the package does not support. A model count that a package
computes in floating point and that is not an integer below 2^64 ends the run
with status 5 and a message on the standard error.

The table states for each package whether `-M` sizes its node table, whether its
caches have a size fixed from `-M` or grow with the work, whether the package
stops itself at `-M`, and what it then prints, on the standard error, and with
which exit status.

| **Package**   | **Node table**                                                                                       | **Caches**                                                                             | **Stops at `-M`**                      | **Message and exit status**                                                   |
|---------------|------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------|----------------------------------------|-------------------------------------------------------------------------------|
| **Aytdd**     | initial capacity from `-M`, past which it grows                                                      | fixed from `-M`                                                                        | no, the table holds 2^31 - 1 decisions | a Lean panic `the node table has overflowed`, 1                               |
| **aytdd_ot**  | as Aytdd                                                                                             | as Aytdd                                                                               | as Aytdd                               | as Aytdd                                                                      |
| **BuDDy**     | fixed from `-M`                                                                                      | fixed from `-M`, six caches                                                            | yes                                    | `BDD error: Number of nodes reached user defined maximum`, 4                  |
| **CUDD**      | grows from a small start, `-M` sets how readily it collects garbage instead of growing               | grow with the hit rate, up to a third of `-M`                                          | no                                     | none                                                                          |
| **LibBDD**    | none shared, `-M` bounds the nodes of all live BDDs after each operation                             | local to each operation                                                                | yes, between operations                | `Too many nodes (n > m)`, 134 (abort)                                         |
| **OxiDD**     | fixed from `-M`                                                                                      | fixed from `-M`                                                                        | yes                                    | a Rust panic `the given function is invalid: OutOfMemory`, 134 (abort)        |
| **RSDD**      | grows without bound                                                                                  | grow without bound                                                                     | no                                     | none                                                                          |
| **Ruddy**     | grows, the C interface bounds it by `-M` after each operation                                        | local to each operation                                                                | yes, between operations                | `ruddy: the node table needs n slots, more than the budget of m`, 134 (abort) |
| **SimpleABC** | fixed from `-M`                                                                                      | fixed from `-M`, a cache of conjunctions                                               | yes                                    | `simpleabc: out of memory, the node table is full`, 4                         |
| **Sylvan**    | grows from a small start up to a maximum from `-M`                                                   | grow with the table up to a maximum from `-M`                                          | yes                                    | `BDD Unique table full, n of m buckets filled!`, 1                            |
| **TBUDDY**    | grows from a size of its own, 200,000 nodes below 1,000 clauses and 2,000,000 from there, up to `-M` | grow with the table, one entry per eight nodes                                         | yes                                    | `BDD error: Number of nodes reached user defined maximum`, 4                  |

The arithmetic by which an adapter turns `-M` into sizes rests on the sizes of
the package's nodes and cache entries, and some of it is inherited from upstream
as it is. The BuDDy adapter assumes nodes of 24 bytes and cache entries of 16,
where BuDDy's are 20 and 24 bytes, and its cache size is always one entry per 32
nodes in each of the six caches, since the memory left by the nodes is computed
but not used. The LibBDD adapter assumes nodes of 16 bytes, where LibBDD's are
12. The tables of Sylvan and SimpleABC have a power of two of entries, so that
they take between half and all of `-M`. SimpleABC counts 30 bytes per slot, or
31 above 254 variables, and not a word in each of the two tables of the vector
compositions, which are allocated only by a composition; a `-M`
that gives fewer slots than the variables plus one is reported by the adapter,
with exit status 1. TBUDDY counts 31 bytes per node, a node of 28 bytes and an
eighth of a cache entry of 24, and a `-M` below its initial table is reported by
the adapter, with exit status 1. Aytdd, whose table grows past `-M`, CUDD, which
takes `-M` as a target rather than a limit, and RSDD, which has no setting for
it, go on until the cap of the run.

## Benchmarks

### Apply

Based on [[Pastva2023](#references)], this benchmark loads two (or more) decision
diagrams stored in a *binary* format (as they are serialized by the
[LibBDD](https://github.com/sybila/biodivine-lib-bdd) library) and then combines
them with an *Apply* operation (or more).

The benchmark can be configured with the following options:

- **`-f <path>`**

  Path to a *.bdd* / *.zdd* file. Use this once for each input. You can find
  some inputs in the *benchmarks/apply* folder together with links to larger
  and more interesting inputs.

- **`-o <and|or>`** (default: *and*)

  Specify the operator to be used to combine the two decision diagrams.

```bash
./build/src/${LIB}_apply_${KIND} -f benchmarks/apply/x0.bdd -f benchmarks/apply/x1.bdd -o and
```


### CNF Solver

This benchmark loads a DIMACS CNF file, constructs its clauses and conjoins them.
The benchmark uses the variable order specified in the CNF file and interprets
the clause list as an approximately balanced tree, e.g.
(c<sub>0</sub> c<sub>1</sub>) (c<sub>2</sub> (c<sub>3</sub> c<sub>4</sub>)). It
makes sense to preprocess raw CNF files using external tools and infer good
variable and clause orders.

The variable order is given by the comment lines before the problem line whose
first word is a number, one variable per line from the top of the diagram down.
If the file gives an order, it must name each variable exactly once, and a file
that names a variable twice, a variable above the number of variables of the
problem line, or a number too large to be a variable is rejected with the
variable in the message. A comment line whose first word is a number after the
problem line is ignored with a warning on the standard error.

```bash
./build/src/${LIB}_cnf_${KIND} -f benchmarks/cnf/sample.cnf
```

The benchmark can be configured with the following options:

- **`-c`**

  Count the satisfying assignments of the conjunction. A package that counts in
  floating point ends the run with status 5 if the count is not an integer below
  2^64, see [Memory](#memory).

- **`-q wmc`**

  Count the weighted models of the conjunction under the weights that the file
  gives its literals in the format of the model counting competition 2022, see
  [benchmarks/wmc](benchmarks/wmc). The count is timed apart from the
  construction and added to the total time:

  ```json
  "count": {
    "query": "wmc",
    "result": "208141/1000000",
    "type": "exact",
    "expected": "208141/1000000",
    "correct": true,
    "time (ms)": 0
  }
  ```

  An exact result is a reduced fraction `num/den`, or an integer, and a result
  in double precision is the shortest decimal that reads back as the `double`.
  Aytdd counts both ways and reports the count in double precision as a second
  block `"count (double)"` of the same form, whose time the total does not
  include; RSDD counts in double precision, which is the weighted model count
  only if every variable that a path to true skips has weights summing to one,
  as on the files of [benchmarks/wmc](benchmarks/wmc). Every other package exits
  with status 3.

  A file may state the count it expects in the comment line `c expected wmc V`,
  where `V` is a decimal of the same form as a weight. The block then also
  reports the expected count, in the form of its result, and whether the result
  is correct: an exact result must equal the expected count, and a result in
  double precision must lie within a relative error of at most 10^-9 of it,
  computed exactly. A block of a file without that line has neither field. A
  weight that is not zero but so small that its nearest `double` is zero is
  rejected, as is an expected count given twice. The option `-q` is rejected
  together with `-c`, `-s` and `-b`, and on a file without the line `c t wmc`.

  ```bash
  ./build/src/aytdd_cnf_bcdd -f benchmarks/wmc/cancer-query.cnf -q wmc
  ```

- **`-s <path>`**

  Instead of the balanced conjunction, follow the schedule in the given file,
  which interleaves conjunctions with existential quantification and decides
  whether the CNF is satisfiable. The format is that of the SAT solver `tbsat`
  of Bryant's TBUDDY package, see [benchmarks/cnf-schedule](benchmarks/cnf-schedule).
  Only one schedule may be given.

  ```bash
  ./build/src/${LIB}_cnf_${KIND} -f benchmarks/cnf-schedule/pigeon-scan-015.cnf -s benchmarks/cnf-schedule/pigeon-scan-015.schedule
  ```

- **`-b`**

  Instead of the balanced conjunction, decide whether the CNF is satisfiable by
  bucket elimination as `tbsat` performs it, see
  [benchmarks/cnf-bucket](benchmarks/cnf-bucket).

  ```bash
  ./build/src/${LIB}_cnf_${KIND} -f benchmarks/cnf-bucket/urquhart-li-03.cnf -b
  ```

- **`-p <path>`**

  Write the proof of unsatisfiability to the given file in the LRAT format. It
  is required for TBUDDY and rejected for every other package. A false result
  comes with a refutation of the CNF, which the checker `lrat-check` of the
  TBUDDY artifact verifies. The count of `-c` is that of BuDDy,
  `bdd_satcountset` over the variables of the CNF, and the proof does not
  certify it.

  ```bash
  ./build/src/tbuddy_cnf_bdd -f benchmarks/cnf-bucket/urquhart-li-03.cnf -b -p urquhart-li-03.lrat
  ```

The options `-s` and `-b` exclude each other and are not supported for ZDDs.
They are rejected together with `-c` and `-q`, since they decide satisfiability
and count nothing, and together with dynamic variable reordering (`-R`), since
both read the variable order as fixed.


### Game Of Life

Solves the following problem:

> Given N<sub>1</sub> and N<sub>2</sub>, how many *Garden of Edens* exist of size
> N<sub>1</sub>xN<sub>2</sub> in Conway's Game of Life?

The benchmark can be configured with the following options:

- **`-n <int>`**

  The size of the sub-game. Use twice for non-quadratic grids.

- **`-s <...>`**

  Restrict the search to *symmetrical* Garden of Edens:

  - `none`: Search through all solutions (default), i.e., no symmetries are
    introduced.

  - `mirror`/`mirror-vertical`: Solutions that are reflected vertically.

  - `mirror-quadrant`/`mirror-quad`: Solutions that are reflected both
    horizontally and vertically.

  - `mirror-diagonal`/`mirror-diag`: Solutions that are reflected across one
    diagonal (requires a square grid).

  - `mirror-double_diagonal`/`mirror-double_diag`: Solutions that are reflected
    across both diagonals (requires a square grid).

  - `rotate`/`rotate-90`: Solutions that are rotated by 90 degrees (requires a
    square grid).

  - `rotate-180`: Solutions that are rotated by 180 degrees.

All symmetries use a variable order where the pre/post variables are zipped and
and follow a row-major ordering.

```bash
./build/src/${LIB}_game-of-life_${KIND} -n 5 -n 4 -s rotate-180
```


### Hamiltonian Cycle

Solves the following problem:

> Given N<sub>1</sub> and N<sub>2</sub>, how many hamiltonian cycles exist on a
> Grid Graph size N<sub>1</sub>xN<sub>2</sub>?

The benchmark can be configured with the following options:

- **`-n <int>`**

  The size of the grid; use twice for non-quadratic grids.

- **`-e <...>`** (default: *time*)

  Pick the encoding/algorithm to solve the problem with:

  - `time`/`t`: Based on [[Bryant2021](#references)], all O(N<sup>4</sup>)
    states are represented as individual variables, i.e., each variable
    represents the position and time. A transition relation then encodes the
    legal moves between two time steps. By intersecting moves at all time steps
    we obtain all paths. On-top of this, hamiltonian constraints are added and
    finally the size of the set of cycles is obtained.

  - `binary`: Based on [[Marijn2021](#references)] with multiple tweaks by
    Randal E. Bryant to adapt it to decision diagrams. Each cell's choice of
    move, i.e., each edge, is represented by a binary number with
    *log<sub>2</sub>(4) = 2* variables. Yet, this does not enforce the choice of
    edges correspond to a Hamiltonian Cycle. Hence, we further add
    *log<sub>2</sub>(N<sup>2</sup>)*-bit binary counter gadgets. These encode
    *if u->v then v=u+1 % N<sup>2</sup>*.

<!--
  - `unary`/`one-hot`: Similar to `binary` but the edges and the gadgets of *b*
    values use a one-hot encoding with *b* variables. Only one out of the *b*
    variables are ever set to true at the same time; the value of the gadget is
    the variable set to true.

  - `crt_one-hot`/`crt`: Similar to `unary` but one or more prime numbers are
    used for the gadgets added at the end. By use of the Chinese Remainder
    Theorem, we can still be sure, we only have valid cycles at the end. One
    hopes this decreases the size of the diagram, since the number of possible
    values for each gadget are much smaller.
-->

  The `time` <!-- and the `unary`/`crt_unary` --> encoding are designed with ZDDs
  in mind whereas the `binary` encoding is designed for BDDs. That is, using the
  `time` encoding with BDDs does not give you great, i.e., small and fast, results.

```bash
./build/src/${LIB}_hamiltonian_${KIND} -n 6 -n 5
```

<!--
> [!IMPORTANT]
> The gadget for the *unary* encodings is not yet properly implemented. Hence,
> this variant has been disabled.
-->


### McNet

> [!IMPORTANT]
> ZDDs are not supported for this benchmark (yet)!

This simple *Model Checker for Nets* (pronounced "*Mac Net*", "*magnet*" or
maybe "*Monet*"?) provides algorithms for the exploration of various transition
system formats.

- **`-f <path>`**

  Path to a file with a Petri Net (*.pnml*) or a Boolean Network (*.aeon*,
  *.bnet*, *.sbml*). You can find multiple inputs in the *benchmarks/mcnet*
  folder together with links to larger and more interesting inputs.

- **`-a <...>`**

  The algorithm(s) to run. All algorithms depend on having computed the
  reachable states.

  - `reachable`/`reachability`: Compute the set of reachable states.

  - `deadlocks`: Compute the set of reachable deadlock states.

  - `scc`: Compute each Strongly Connected Component using the Chain
    algorithm from [[Larsen2023](#references)]

- **`-o <...>`** (default: *input*)

  Each BDD variable represents a location in the net. Hence, the variable
  ordering can have a major impact on the diagrams' size.

  - `cuthill-mckee`: Use the Cuthill-McKee order derived from the incidence
    graph, i.e. where variables are connected if they occur together in the
    same transition.

  - `input`: Use the order in which the locations are declared in the file.

  - `random`: A randomized ordering of variables.

  - `sloan`: Use Sloan's ordering derived from the incidence graph, where
    variables are connected if they occur together in the same transition.

- **`-s <...>`**

  Merges the transition relation together into one with the given semantics.
  If not set, then the model is explored with *asynchronous* semantics and a
  *disjoint* relation.

  - `asynchronous`: Explore the model instead with *asynchronous* update
    semantics and a single *joined* relation.

  - `synchronous`: Explore the model instead with *synchronous* update
    semantics.

```bash
./build/src/${LIB}_mcnet_${KIND} -f benchmarks/mcnet/split.pnml
```


### Picotrav
This benchmark is a small recreation of the *Nanotrav* example provided with the
CUDD library [[Somenzi2015](#references)]. Given a hierarchical circuit in (a
subset of the) [Berkeley Logic Interchange Format
(BLIF)](https://course.ece.cmu.edu/~ee760/760docs/blif.pdf) a BDD is created for
every net; dereferencing BDDs for intermediate nets after they have been used
for the last time. If two *.blif* files are given, then BDDs for both are
constructed and every output net is compared for equality.

The benchmark can be configured with the following options:

- **`-f <path>`**

  Path to a *.blif* file. You can find multiple inputs in the
  *benchmarks/picotrav* folder together with links to larger and more
  interesting inputs.

  If used twice, the BDDs for both circuits' output gates are constructed and
  checked for logical equality.

- **`-m order|name`** (default: *order*)

  In case two circuits are given as inputs to verify their equivalence, match
  the inputs and outputs of these circuits by the *order* in which they occur in
  the input files or by their *names*.

- **`-o <...>`** (default: *input*)

  BDD variables represent the value of inputs. Hence, a good variable ordering
  is important for a high performance. To this end, one can use various variable
  orderings derived from the given net.

  - `input`: Use the order in which they are declared in the input *.blif* file.

  - `df`/`depth-first`: Variables are ordered based on a depth-first traversal
    where non-input gates are recursed to first; thereby favouring deeper nodes.

  - `df_level`/`depth-first_level`: Similar to `depth-first`, but before
    recursing, all child nodes are sorted by their depth. Recursive calls are
    done from the deepest to the most shallow node in order.

  - `fanin`: Order variables based on the fanin of the input gates. That is, the
    "most referenced" variables come first.

  - `fanin_df`: Similar to `fanin` but ties are broken based on a `df` order.

  - `fanin_df_level`: Similar to `fanin` but ties are broken with the `df_level`
    order.

  - `fujita`: The variable ordering from [[Fujita1998](#references)]. It is
    similar to `df_level`, but input gates are sorted based on their fanin, i.e.
    the number of references to the gate.

  - `level`: Variables are ordered based on the deepest reference by another
    net. Ties are broken based on the declaration order in the input (`input`).

  - `level_df`/`level_depth-first`: Similar to `level` but ties are broken based
    on the ordering in `df` rather than `input`.

  - `random`: A randomized ordering of variables.

  - `zip`: Assuming that the input variable order is `a[0]`, `a[1]`, …,
    `a[n-1]`, `b[0]`, `b[1]`, …, `b[n-1]` in the input *.blif* file, use the
    order `a[0]`, `b[0]`, `a[1]`, `b[1]`, …, `a[n-1]`, `b[n-1]`.

```bash
./build/src/${LIB}_picotrav_${KIND} -f benchmarks/picotrav/not_a.blif -f benchmarks/picotrav/not_b.blif -o df_level
```


### QBF Solver

> [!IMPORTANT]
> ZDDs are not supported for this benchmark (yet)!

This benchmark is a small recreation of Jaco van de Pol's
[Qubi](https://github.com/jacopol/qubi) project. Given a Quantified Boolean
Formula (QBF) in the [*qcir*](https://www.qbflib.org/qcir.pdf) format, the
decision diagram representing each gate is computed bottom-up. The outermost
quantifier in the *prenex* is not resolved with BDD operations. Instead, if the
decision diagram has not already collapsed to a terminal, a witness or a
counter-example is obtained from the diagram.

- **`-f <path>`**

  Path to a *.qcir* file. You can find multiple inputs in the *benchmarks/qbf*
  folder together with links to larger and more interesting inputs.

- **`-o <...>`** (default: *input*)

  Each BDD variable corresponds to an input variables, i.e., a literal, of the
  *qcir* circuit. To keep the decision diagrams small, one can choose from using
  different variable orders; each of these are derived from the given circuit
  during initialisation.

  - `input`: Use the order in which variables are introduced in the *.qcir*
    file.

  - `df`/`depth-first`: Order variables based on their first occurence in a
    depth-first traversal of the circuit.

  - `df_rtl`/`depth-first_rtl`: Same as `df` but the depth-first traversal is
    *right-to-left* for each gate in the circuit.

  - `level`/`level_df`: Order variables first based on their deepest reference
    by another gate. Ties are broken based on the depth-first (`df`) order.

  If the `input` ordering is used, then the gates of the circuit are also
  resolved in the order they were declared. Otherwise, for `df` and `level`,
  gates are resolved in a bottom-up order based on their depth within the
  circuit.

```bash
./build/src/${LIB}_qbf_${KIND} -f benchmarks/qbf/example_a.qcir -o df
```


### Queens
Solves the following problem:

> Given N, in how many ways can N queens be placed on an N x N chess board
> without threatening eachother?

Our implementation of these benchmarks are based on the description of
[[Kunkle2010](#references)]. Row by row, we construct an BDD that represents
whether the row is in a legal state: is at least one queen placed on each row
and is it also in no conflicts with any other? On the accumulated BDD we then
count the number of satisfying assignments.

The benchmark can be configured with the following options:

- **`-n <int>`**

  The size of the chess board.

```bash
./build/src/${LIB}_queens_${KIND} -n 8
```


### RelProd

> [!IMPORTANT]
> ZDDs are not supported for this benchmark (yet)!

Building on-top of the [Apply](#apply) benchmark, this benchmark loads a
*relation* and a set of *states* stored in a *binary* format (as they are
serialized by the
[LibBDD](https://github.com/sybila/biodivine-lib-bdd) library) and then
combines them with a *Relational Product* operation in either direction.

The benchmark can be configured with the following options:

- **`-o <next|prev>`** (default: *next*)

  Specify whether the transition relation should be traversed forwards
  (*next*) or backwards (*prev*).

- **`-r <path>`**

  Path to a *.bdd* / *.zdd* file that contains the relation. It is also assumed,
  that this relation includes the *frame rule*.

- **`-s <path>`**

  Path to a *.bdd* / *.zdd* file that contains the set of states.

```bash
./build/src/${LIB}_relprod_${KIND} -r benchmarks/relprod/self-loop/relation.bdd -s benchmarks/relprod/self-loop/states_all.bdd -o next
```


### Tic-Tac-Toe
Solves the following problem:

> Given N, in how many ways can Player 1 place N crosses in a 3D 4x4x4 cube and
> have a tie, when Player 2 places noughts in all remaining positions?

This benchmark stems from [[Kunkle2010](#references)]. Here we keep an
accumulated BDD on which we add one of the 76 constraints of at least one cross
and one nought after the other. We add these constraints in a different order
than [[Kunkle2010](#references)], which does result in an up to 100 times
smaller largest intermediate result.

The interesting thing about this benchmark is, that even though the BDDs grow
near-exponentially, the initial BDD size grows polynomially with N, it always
uses 64 variables number and 76 Apply operations.

- **`-n <int>`**

  The number of crosses are placed in the 64 positions.

```bash
./build/src/${LIB}_tic-tac-toe_${KIND} -n 20
```


## Performance Regression Testing

This collection of benchmarks is not only a good way to compare implementations
of BDD packages. Assuming there are no breaking changes in a pull request, this
also provides a way to test for *performance regression*.

To this end, *regression.py* provides an interactive python program that fully
automates downloading inputs and running and analysing the benchmark timings of
two branches. For a non-interactive context, e.g., continuous integration, all
arguments can be parsed through *stdin*. For example, the following goes through
all the prompts for using the *8*-Queens problem to test Adiar with *128* MiB
`origin/main` (against itself) with *no* verbose build or runtime output and
with at least 3 and at most 10 samples.

```bash
python regression.py <<< "queens
8
adiar
128
origin
main
origin
main
no
no
3
10
"
```

Between *min* and *max* number of samples are collected, until both the
*baseline* and the *under test* branches have a standard deviation below 5%. For
smaller instances, samples are collected for at least 30 minutes whereas for
larger instances, the last sampling may start after 90 minutes.

The python script exits with a non-zero code, if there is a statistically
relevant slowdown. A short and concise report is placed in
*regression_{package}.out* which can be posted as a comment on the pull request.


## License
The software files in this repository are provided under the
[MIT License](/LICENSE.md).


## Citation
If you use this repository in your work, we sadly do not yet have written a
paper on this repository alone (but is planned). In the meantime, please cite
the initial paper on *Adiar*.

```bibtex
@InProceedings{soelvsten2022:TACAS,
  title         = {{Adiar}: Binary Decision Diagrams in External Memory},
  author        = {S{\o}lvsten, Steffan Christ
               and van de Pol, Jaco
               and Jakobsen, Anna Blume
               and Thomasen, Mathias Weller Berg},
  year          = {2022},
  booktitle     = {Tools and Algorithms for the Construction and Analysis of Systems},
  editor        = {Fisman, Dana
               and Rosu, Grigore},
  pages         = {295--313},
  numPages      = {19},
  publisher     = {Springer},
  series        = {Lecture Notes in Computer Science},
  volume        = {13244},
  isbn          = {978-3-030-99527-0},
  doi           = {10.1007/978-3-030-99527-0\_16},
}
```

## Publications

The following list of publications (given in order of publication date) have
used this benchmarking suite for their evaluation:

- Steffan Christ Sølvsten, Jaco van de Pol, Anna Blume Jakobsen, and Mathias
  Weller Berg Thomasen. “*Adiar: Binary Decision Diagrams in External Memory*”.

  [arXiv](https://arxiv.org/abs/2104.12101),
  [TACAS 2022](https://link.springer.com/chapter/10.1007/978-3-030-99527-0_16)

- Steffan Christ Sølvsten and Jaco van de Pol.
  “*Adiar 1.1: Zero-suppressed Decision Diagrams in External Memory*”.

  [NFM 2023](https://link.springer.com/chapter/10.1007/978-3-031-33170-1_28)

- Steffan Christ Sølvsten and Jaco van de Pol.
  “*Predicting Memory Demands of BDD Operations using Maximum Graph Cuts*”.

  [arXiv](https://arxiv.org/abs/2307.04488),
  [ATVA 2023](https://link.springer.com/chapter/10.1007/978-3-031-45332-8_4)

- Nils Husung, Clemens Dubslaff, Holger Hermanns, and Maximilian A. Köhl.
  “*OxiDD: A Safe, Concurrent, Modular, and Performant Decision Diagram
  Framework in Rust*”.

  [TACAS 2024](https://link.springer.com/chapter/10.1007/978-3-031-57256-2_13)

- Steffan Christ Sølvsten, Casper Moldrup Rysgaard, and Jaco van de Pol.
  “*Random Access on Narrow Decision Diagrams in External Memory*”.

  [SPIN 2024](https://link.springer.com/chapter/10.1007/978-3-031-66149-5_7)

- Steffan Christ Sølvsten and Jaco van de Pol.
  “*Multi-variable Quantification of BDDs in External Memory using Nested
  Sweeping*”.

  [VMCAI 2026](https://link.springer.com/chapter/10.1007/978-3-032-15700-3_17),
  [arXiv](https://arxiv.org/abs/2408.14216)

- Steffan Christ Sølvsten and Jaco van de Pol.
  “*Symbolic Model Checking in External Memory*”.

  [arXiv](https://arxiv.org/abs/2505.11229)

## References

- [[Arge1995](https://link.springer.com/chapter/10.1007/BFb0015411)]
  Lars Arge. “*The I/O-complexity of Ordered Binary-decision Diagram
  Manipulation*”. In: *Proceedings of International Symposium on Algorithms and
  Computations*. (1995)

- [[Arge1996](https://www.brics.dk/RS/96/28/)]
  Lars Arge. “*The Buffer Tree: A new technique for optimal I/O-algorithms*”.
  In: *Algorithms and Data Structures*. (1995)

- [[Ashar1994](https://ieeexplore.ieee.org/document/629886)]
  Pranav Ashar and Matthew Cheong: “*Efficient Breadth-first Manipulation of
  Binary Decision Diagrams*”. In: *IEEE/ACM International Conference on
  Computer-Aided Design*. (1994)

- [[Beneš2020](https://link.springer.com/chapter/10.1007/978-3-030-53288-8_28)]
  Nikola Beneš, Luboš Brim, Jakub Kadlecaj, Samuel Pastva, and David Šafránek:
  “*AEON: Attractor Bifurcation Aanalysis of Parametrised Boolean Networks*”. In
  *Computer Aided Verification*. (2020)

- [[Brace1990](https://doi.org/10.1109/DAC.1990.114826)]
  K. Brace, R. Rudell, R. E. Bryant: “*Efficient implementation of a BDD package*”.
  In: *27th ACM/IEEE Design Automation Conference*. (1990)

- [[Bryant1986](https://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=1676819)]
  R. E. Bryant. “*Graph-Based Algorithms for Boolean Function Manipulation*”.
  In: *IEEE Transactions on Computers*. (1986)

- [[Bryant2021](https://github.com/rebryant/Cloud-BDD/blob/conjunction_streamlined/hamiltonian/hpath.py)]
  R. E. Bryant. “*hpath.py*”. In: *GitHub/Cloud-BDD*. (2021)

- [[Bryant2022](https://www.cs.cmu.edu/~bryant/pubdir/fmcad22.pdf)]
  R. E. Bryant. “*TBUDDY: A Proof-Generating BDD Package*”. In: *Formal Methods
  in Computer-Aided Design (FMCAD)*. (2022)

- [[Chaing1995](https://dl.acm.org/doi/10.5555/313651.313681)]
  Yi-Jen Chiang, Michael T. Goodrich, Edward F. Grove, Roberto Tamassia, Darren
  Erik Vengroff, and Jeffrey Scott Vitter. “*External-memory Graph Algorithm*”.
  In: *Proceedings of the Sixth Annual ACM-SIAM Symposium on Discrete
  Algorithms*. (1995)

- [[Fujita1998](https://ieeexplore.ieee.org/document/122450)]
  Fujita, H. Fujisawa, and N. Kawato. “*Evaluation and improvement of Boolean
  comparison method based on Binary Decision Diagrams*”. In: *International
  Conference on Computer-Aided Design *. (1998)

- [[Husung2024](https://link.springer.com/chapter/10.1007/978-3-031-57256-2_13)]
  Nils Husung, Clemens Dubslaff, Holger Hermanns, and Maximilian A. Köhl:
  “*OxiDD: A Safe, Concurrent, Modular, and Performant Decision Diagram
  Framework in Rust*”. In: *Tools and Algorithms for the Construction and
  Analysis of Systems*. (2024)

- [[Larsen2023](https://link.springer.com/chapter/10.1007/978-3-031-30820-8_22)]
  C. A. Larsen, S. M. Schmidt, J. Steensgaard, A. B. Jakobsen, J. van de Pol,
  and A. Pavlogiannis: “*A Truly Symbolic Linear-Time Algorithm for SCC
  Decomposition*”. In: *27th ACM/IEEE Design Automation Conference*.
  (1990)

- [[Lind-Nielsen1999](https://www.itu.dk/research/buddy/)]
  Jørn Lind-Nielsen: “*BuDDy: A Binary Decision Diagram Package*”. Department of
  Information Technology, Technical University of Denmark. (1999)

- [[Kunkle2010](https://dl.acm.org/doi/abs/10.1145/1837210.1837222)] Daniel
  Kunkle, Vlad Slavici, Gene Cooperman. “*Parallel Disk-Based Computation for
  Large, Monolithic Binary Decision Diagrams*”. In: *PASCO '10: Proceedings of
  the 4th International Workshop on Parallel and Symbolic Computation*. (2010)

- [[Marijn2021](https://link.springer.com/chapter/10.1007/978-3-030-80223-3_15)]
  Heule, Marijn J. H. “*Chinese Remainder Encoding for Hamiltonian Cycles*”. In:
  *Theory and Applications of Satisfiability Testing*. (2021)

- [[Minato1993](https://dl.acm.org/doi/10.1145/157485.164890)]
  S. Minato. “*Zero-suppressed BDDs for Set Manipulation in Combinatorial
  Problems*”. In: *International Design Automation Conference*. (1993)

- [[Miyasaka2019](https://github.com/MyskYko/abc-bdd)]
  Yukio Miyasaka, Alan Mishchenko, and Masahiro Fujita: “*A Simple BDD Package
  without Variable Reordering and Its Application to Logic Optimization with
  Permissible Functions*”. In: *International Workshop on Logic & Synthesis*.
  (2019)

- [[Ochi1993](https://dl.acm.org/doi/10.5555/259794.259803)]
  Hiroyuki Ochi, Koichi Yasuoka, and Shuzo Yajima: “*Breadth-first Manipulation
  of very Large Binary-Decision Diagrams*”. In: *International Conference on
  Computer Aided Design*. (1993)

- [[Pastva2023](https://ieeexplore.ieee.org/document/10329375)]
  S. Pastva and T. Henzinger. “*Binary Decision Diagrams on Modern Hardware*”.
  In: *Proceedings of the 23rd Conference on Formal Methods in Computer-Aided
  Design*. (2023)

- [[Sanghavi1996](https://dl.acm.org/doi/10.1145/240518.240638)]
  Jagesh V. Sanghavi, Rajeev K. Ranjan, Robert K. Brayton, and Alberto
  Sangiovanni-Vincentelli: “*High performance BDD package by Exploiting Memory
  Hierarchy*”. In: *33rd Design Automation Conference*. (1996)

- [[Somenzi2015](https://github.com/ssoelvsten/cudd)]
  Somenzi, Fabio: *CUDD: CU Decision Diagram Package, 3.0*. University
  of Colorado at Boulder (2015)

- [[Sølvsten2022](https://link.springer.com/chapter/10.1007/978-3-030-99527-0_16)]
  Steffan Christ Sølvsten, Jaco van de Pol, Anna Blume Jakobsen, and Mathias
  Weller Berg Thomasen. “*Adiar: Binary Decision Diagrams in External Memory*”. In:
  *Tools and Algorithms for the Construction and Analysis of Systems*. (2022)

- [[Dijk2016](https://link.springer.com/article/10.1007/s10009-016-0433-2)]
  Tom van Dijk and Jaco van de Pol: “*Sylvan: Multi-core Framework for Decision
  Diagrams*”. In: *International Journal on Software Tools for Technology
  Transfer* (2016)
