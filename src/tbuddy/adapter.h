#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../common/adapter.h"

#include <tbdd.h>
// After `tbdd.h`, which declares the proof types
#include <prover.h>

////////////////////////////////////////////////////////////////////////////////
/// TBUDDY (Bryant, FMCAD 2022) extends BuDDy with trusted BDDs: a BDD together
/// with the identifier of a clause, in an LRAT proof written alongside the
/// computation, stating that the input clauses imply the BDD. Conjunction
/// (`tbdd_and`) and validation of an implied BDD (`tbdd_validate`) extend the
/// proof, so that deriving the constant false derives the empty clause and
/// thereby a refutation of the input clauses.
///
/// This adapter runs the CNF benchmark through these operations, so its timings
/// include the generation of the proof. Only the CNF benchmark is built for it,
/// as the proof certifies nothing but implications of the input clauses.
///
/// - tbdd_init:
///     Takes the input clauses and the variable order, writes the proof's
///     premises, and initialises BuDDy itself, with a table size chosen from the
///     number of clauses (`prover.c`), which then grows with the computation.
///     It is called from `init_proof`, once the CNF is parsed.
///
/// - bdd_setmaxnodenum:
///     Bounds the growth of the table by the nodes that fit into `-M`. A node
///     takes 28 bytes, as it carries the extension variable and the defining
///     clause of the proof (`BddNode` in `kernel.h`), and `tbdd_init` gives the
///     one operator cache an entry of 24 bytes (`BddCacheData` in `cache.h`)
///     per 8 nodes of the table, which it keeps as the table grows (`prover.c`),
///     so that a node takes 31 bytes in all.
///
/// - tbdd_done:
///     Not called. It prints statistics to standard output, where the JSON of
///     the benchmark goes, and otherwise only deletes the unit clauses still
///     alive, which an LRAT proof need not do.
///
/// BuDDy's variable `x` is the CNF variable `x`, variable 0 is unused, and the
/// order puts the variable of the benchmark's level `l` at BuDDy's level `l+1`.
////////////////////////////////////////////////////////////////////////////////

/// Memory per node of the table of TBUDDY, its share of the operator cache included
constexpr size_t tbuddy_bytes_per_node = 31u;

/// The error handler installed by `bdd_init`, to which `tbuddy_error_handler` leaves every error
/// other than the lack of a node or of memory.
static bddinthandler tbuddy_default_error_handler = nullptr;

/// Ends the run as out of memory when TBUDDY finds no free node in a table that has reached the
/// maximum set from `-M`, or fails to allocate memory, and hands every other error to the handler
/// installed before.
inline void
tbuddy_error_handler(int e)
{
  if (e == BDD_NODENUM || e == BDD_MEMORY) {
    exit_out_of_memory(std::string("BDD error: ") + bdd_errstring(e));
  }
  if (tbuddy_default_error_handler) { tbuddy_default_error_handler(e); }
}

/// A trusted BDD of TBUDDY. The functions of `tbdd.h` take non-constant
/// references, hence the mutable member.
class tbuddy_function
{
  mutable trustbdd::tbdd _f;

public:
  tbuddy_function() = default;

  tbuddy_function(const trustbdd::tbdd& f)
    : _f(f)
  {}

  /// The underlying BDD
  bdd
  root() const
  {
    return _f.get_root();
  }

  trustbdd::tbdd&
  get() const
  {
    return _f;
  }

  tbuddy_function
  operator&(const tbuddy_function& g) const
  {
    return trustbdd::tbdd_and(_f, g._f);
  }

  bool
  operator==(const tbuddy_function& g) const
  {
    return root() == g.root();
  }

  bool
  operator!=(const tbuddy_function& g) const
  {
    return !(*this == g);
  }
};

class tbuddy_bdd_adapter
{
public:
  static constexpr std::string_view name = "TBUDDY";
  static constexpr std::string_view dd   = "BDD";

  static constexpr bool needs_extend     = false;
  static constexpr bool needs_frame_rule = true;

  static constexpr bool complement_edges = false;

  /// The operations generate a clausal proof (see `init_proof`)
  static constexpr bool proof_generating = true;

public:
  typedef tbuddy_function dd_t;

private:
  const int _varcount;

  /// CNF variable at each level of the benchmark
  std::vector<int> _level_to_var;

  /// Counters that TBUDDY advances as it adds extension variables and clauses
  int _variable_counter = 0;
  int _clause_counter   = 0;

  FILE* _proof = nullptr;

  // Init and Deinit
public:
  tbuddy_bdd_adapter(int varcount)
    : _varcount(varcount)
  {}

  ~tbuddy_bdd_adapter()
  {
    if (_proof) {
      bdd_done();
      std::fclose(_proof);
    }
  }

  /// Initialise TBUDDY for a proof, written to `path` in the LRAT format,
  /// whose premises are `clauses`, over the CNF variables `1` to `varcount`,
  /// and order the variables by `level_to_var`.
  ///
  /// Returns `false` on failure.
  bool
  init_proof(const std::vector<std::vector<int>>& clauses,
             const std::vector<int>& level_to_var,
             const std::string& path)
  {
    assert(level_to_var.size() == static_cast<size_t>(_varcount));
    _level_to_var = level_to_var;

    _proof = std::fopen(path.c_str(), "w");
    if (!_proof) {
      std::cerr << "Cannot open the proof file '" << path << "'\n";
      return false;
    }

    std::vector<ilist> input(clauses.size());
    for (size_t i = 0; i < clauses.size(); ++i) {
      input[i] = ilist_new(clauses[i].size());
      for (const int literal : clauses[i]) input[i] = ilist_push(input[i], literal);
    }
    ilist order = ilist_new(_varcount);
    for (const int var : level_to_var) order = ilist_push(order, var);

    _variable_counter = _varcount;
    _clause_counter   = clauses.size();

    tbdd_set_verbose(0);
    const int code = tbdd_init(
      _proof, &_variable_counter, &_clause_counter, input.data(), order, PROOF_LRAT, false);

    // `tbdd_init` copies the clauses and reads the order only during the call
    for (ilist clause : input) ilist_free(clause);
    ilist_free(order);

    if (code != 0) {
      std::cerr << "TBUDDY initialisation failed with code " << code << "\n";
      return false;
    }

    // `bdd_setmaxnodenum` refuses a maximum that is not above the size of the table by calling the
    // error handler, which ends the run, so a smaller '-M' is reported here instead.
    const size_t mib       = 1024 * 1024;
    const size_t max_nodes = std::min<size_t>(static_cast<size_t>(M) * mib / tbuddy_bytes_per_node,
                                              std::numeric_limits<int>::max());
    const size_t allocated = bdd_getallocnum();
    if (max_nodes <= allocated) {
      // The least '-M' whose nodes exceed the table
      const size_t start_mib = (tbuddy_bytes_per_node * (allocated + 1) + mib - 1) / mib;
      std::cerr << "tbuddy: -M below the " << start_mib << " MiB the package allocates at start\n";
      std::exit(1);
    }
    bdd_setmaxnodenum(static_cast<int>(max_nodes));
    tbuddy_default_error_handler = bdd_error_hook(tbuddy_error_handler);

    // Disable the default gbc_handler installed by `bdd_init`, which prints on standard output
    bdd_gbc_hook(NULL);
    return true;
  }

  /// The trusted BDD of input clause `id`, numbered from 1
  inline dd_t
  clause(int id)
  {
    return trustbdd::tbdd(tbdd_from_clause_id(id));
  }

  /// Number of clauses in the proof so far, the input clauses included
  inline size_t
  proof_clauses() const
  {
    return total_clause_count;
  }

public:
  template <typename F>
  int
  run(const F& f)
  {
    return f();
  }

  // BDD Operations
public:
  inline dd_t
  top()
  {
    return trustbdd::tbdd_tautology();
  }

  /// The constant false. It carries no proof; a false result of `&` does.
  inline dd_t
  bot()
  {
    return trustbdd::tbdd_null();
  }

  /// Existential quantification over the levels `[rbegin, rend)`, given in
  /// descending order, justified by the implication of the result by `f`
  ///
  /// `bdd_makeset` conjoins the variables from the last to the first, so they
  /// are listed by ascending level for the cube to be built bottom-up, without
  /// intermediate nodes, each of which would add clauses to the proof.
  template <typename IT>
  inline dd_t
  exists(const dd_t& f, IT rbegin, IT rend)
  {
    std::vector<int> vars;
    while (rbegin != rend) { vars.push_back(_level_to_var[*(rbegin++)]); }
    std::reverse(vars.begin(), vars.end());
    const bdd result = bdd_exist(f.root(), bdd_makeset(vars.data(), vars.size()));
    return trustbdd::tbdd_validate(result, f.get());
  }

  inline uint64_t
  nodecount(const dd_t& f)
  {
    const uint64_t c = bdd_nodecount(f.root());
    // BuDDy does not count terminal nodes. If a BDD has no inner nodes, then it consists of a
    // single terminal node. Otherwise, both terminals are referenced.
    return c == 0 ? 1 : c + 2;
  }

  /// Not certified, and rejected by the benchmark; present for compilation.
  inline uint64_t
  satcount(const dd_t& f)
  {
    // Variable 0 and the extension variables of the proof are not CNF variables
    return checked_satcount(bdd_satcountset(f.root(), cnf_variables()));
  }

  /// The literals of a path to true, by the benchmark's levels
  ///
  /// The path is read off the nodes of `f`, rather than built by `bdd_satone`,
  /// since every node TBUDDY creates adds its defining clauses to the proof.
  /// Like `bdd_satone`, it takes the low child unless that is false.
  inline std::vector<std::pair<int, char>>
  pickcube(const dd_t& f)
  {
    std::vector<std::pair<int, char>> res;
    bdd node = f.root();

    while (node != bddfalse && node != bddtrue) {
      const bdd low     = bdd_low(node);
      const bool go_low = low != bddfalse;
      res.push_back({ bdd_var2level(bdd_var(node)) - 1, go_low ? '0' : '1' });

      node = go_low ? low : bdd_high(node);
    }
    return res;
  }

private:
  bdd
  cnf_variables()
  {
    std::vector<int> vars(_level_to_var);
    return bdd_makeset(vars.data(), vars.size());
  }

  // Statistics
public:
  inline size_t
  allocated_nodes()
  {
    return bdd_getnodenum();
  }

  void
  print_stats()
  {
    std::cout << "\nTBUDDY statistics:\n";

    bddStat stats;
    bdd_stats(&stats);

    std::cout << "   Table:\n"
              << "   | total produced:      " << stats.produced << "\n"
              << "   Garbage Collections:   " << stats.gbcnum << "\n"
              << "   Proof clauses:         " << proof_clauses() << "\n";
  }
};
