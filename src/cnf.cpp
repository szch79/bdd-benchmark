// Assertions
#include <algorithm>
#include <cassert>

// Data structures
#include <cstddef>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

// Types
#include <cstdlib>

// For reading the CNF file
#include <filesystem>
#include <fstream>

#include "common/adapter.h"
#include "common/chrono.h"
#include "common/input.h"
#include "common/json.h"

#ifdef BDD_BENCHMARK_STATS
size_t largest_bdd = 0;
size_t total_nodes = 0;
#endif // BDD_BENCHMARK_STATS

// ========================================================================== //
std::string file;
bool satcount = false;
std::string schedule_file;
bool bucket_elimination = false;
std::string proof_file;

class parsing_policy
{
public:
  static constexpr std::string_view name = "CNF";
  static constexpr std::string_view args = "bf:cp:s:";

  static constexpr std::string_view help_text =
    "        -b                    Conjoin and quantify by bucket elimination\n"
    "        -c                    Count satisfying assignments\n"
    "        -f PATH               Path to '.cnf'/'.dimacs' file\n"
    "        -p PATH               Write the proof of unsatisfiability to PATH (TBUDDY only)\n"
    "        -s PATH               Conjoin and quantify as the schedule file says";

  // NOTE: One could add more options to this benchmark, e.g., to influence the
  // order in which the clauses are conjoined. The current implementation
  // interprets the linear clause ordering given in the input file as an
  // (approximately) balanced binary tree, e.g., `(c0 ∧ c1) ∧ (c2 ∧ (c3 ∧ c4))`.
  // Importantly, it does not commute any operands, i.e., we do not conjoin `c0`
  // and `c4` (or any dependant intermediate results) before `c1` has been
  // processed.
  //
  // In general, this benchmark allows quite important options to be tuned by
  // preprocessing the CNF. We can, e.g., apply preprocessing techniques from
  // #SAT solving using the tool
  // [pmc](https://www.cril.univ-artois.fr/KC/pmc.html), and compute variable
  // and clause orderings using [MINCE](http://www.aloul.net/Tools/mince/).
  // Implementing the algorithms of these tools here would require adding a lot
  // of code, or at least quite a few dependencies (a SAT solver, a hypergraph
  // cutter). Also, we want to be sure that we actually give the same clauses
  // in the same order to the different libraries. Hence, we would need to
  // ensure that the algorithms are deterministic. Therefore it is much easier
  // (and also less time-consuming) to do all the preprocessing steps
  // externally.
  //
  // The options `-s` and `-b` replace the balanced conjunction by the two
  // methods of the SAT solver `tbsat` of Bryant's TBUDDY package
  // (https://github.com/rebryant/tbuddy-artifact), which interleave the
  // conjunctions with existential quantification and therefore decide only
  // whether the CNF is satisfiable. With `-s`, a schedule file lists the
  // operations; with `-b`, bucket elimination orders them by the variable order.
  //
  // A package whose operations generate a clausal proof, TBUDDY, writes it to
  // the path given by `-p`. Its operations prove that the input clauses imply
  // each intermediate result, so a false result comes with a refutation, while
  // a model count is not certified and `-c` is rejected for it.

  /// Parse one command line option, where `c` is one of keys specified in the
  /// `args` constant of this class and `arg` is the option's value (if the
  /// option takes one).
  ///
  /// Returns `true` on error.
  static inline bool
  parse_input(const int c, const char* arg)
  {
    switch (c) {
    case 'f': {
      if (!std::filesystem::exists(arg)) {
        std::cerr << "File '" << arg << "' does not exist\n";
        return true;
      }
      if (!file.empty()) {
        std::cerr << "Only one file may be given\n";
        return true;
      }

      file = arg;
      return false;
    }
    case 'c': {
      satcount = true;
      return false;
    }
    case 's': {
      if (!std::filesystem::exists(arg)) {
        std::cerr << "File '" << arg << "' does not exist\n";
        return true;
      }
      schedule_file = arg;
      return false;
    }
    case 'b': {
      bucket_elimination = true;
      return false;
    }
    case 'p': {
      proof_file = arg;
      return false;
    }
    default: return true;
    }
  }
};

// ========================================================================== //

/// Helper function to compute `|value|` as `unsigned` without causing undefined
/// behavior on `INT_MIN`
unsigned
unsigned_abs(int value)
{
  // from https://stackoverflow.com/a/32676663
  return value < 0 ? 0 - unsigned(value) : unsigned(value);
}

class CNF
{
  /// Literals of all clauses
  std::vector<int> _clause_data;

  /// Offsets of the clauses in `_clause_data`
  ///
  /// If there is at least one clause, then `_clause_offsets[0]` is 0. The size
  /// of this vector is exactly the number of clauses.
  std::vector<size_t> _clause_offsets;

  /// Map from variables to levels
  std::vector<unsigned> _var_to_level;

public:
  /// Parse a DIMACS CNF file from `path`
  ///
  /// A sample file might look like this:
  ///
  /// ```
  /// c 2 b
  /// c 1 a
  /// c 3 c
  /// c 4 d
  /// p cnf 4 4
  /// -1 2 0
  /// -2 1 0
  /// 3 0
  /// -4 0
  /// ```
  ///
  /// The comment lines (starting with `c`) are used to specify the variable
  /// order. Here, variable 2 called "b" is at the top, then variable 1 "a"
  /// follows, and so on. The variable order is optional but if present, if must
  /// contain all variables.
  static std::optional<CNF>
  parse_dimacs_cnf(const std::string& path)
  {
    size_t nclauses;
    CNF cnf;
    std::ifstream input(path, std::ios::in);

    // Parse the variable order
    std::vector<unsigned> var_order;
    std::string tok;
    unsigned line = 0;
    while (input.good()) {
      const int c = input.get();
      ++line;
      if (c == 'c') {
        // Read the comment line as a whole, so that an empty one does not consume the next line
        std::string comment;
        std::getline(input, comment);
        std::istringstream words(comment);
        if (words >> tok && std::all_of(tok.begin(), tok.end(), ::isdigit)) {
          // Ignore lines where the token after `c` is not numeric. This allows extensions of the
          // format.

          const unsigned var_id = std::stoul(tok);
          if (var_id == 0) {
            std::cerr << "error: variable numbers must be > 0 (in variable order)\n";
            return {};
          }
          var_order.push_back(var_id - 1);
        }
      } else if (c == 'p') {
        break;
      } else {
        std::cerr << "error: unexpected character '" << static_cast<char>(c)
                  << "' at beginning of line" << line << "\n";
        return {};
      }
    }

    // Read the problem line (`p cnf <#vars> <#clauses>`)
    std::string problem_type;
    unsigned nvars;
    input >> problem_type >> nvars >> nclauses;

    if (input.fail()) {
      std::cerr << "error: expected `p cnf #vars #clauses` (line " << line << ")\n";
      return {};
    }
    if (problem_type != "cnf") {
      std::cerr << "error: can only handle 'cnf' files\n";
      return {};
    }
    if (nvars >= unsigned(std::numeric_limits<int>::max())) {
      std::cerr << "error: too many variables\n";
      return {};
    }
    if (var_order.size() == 0) { // Allow no variable order to be given
      cnf._var_to_level.reserve(nvars);
      for (unsigned i = 0; i < nvars; i++) { cnf._var_to_level.push_back(i); }
    } else { // Case where a variable order is given via the comment lines
      if (nvars != var_order.size()) {
        std::cerr << "error: number of variables does not match\n";
        return {};
      }

      constexpr unsigned NO_VAR = std::numeric_limits<unsigned>::max();
      cnf._var_to_level.resize(nvars, NO_VAR);
      for (unsigned i = 0; i < nvars; i++) {
        if (cnf._var_to_level[var_order[i]] != NO_VAR) {
          std::cerr << "error: variable " << cnf._var_to_level[var_order[i]]
                    << "occurs twice in order\n";
          return {};
        }
        cnf._var_to_level[var_order[i]] = i;
      }

      // Holds by the pigeonhole principle
      assert(std::all_of(cnf._var_to_level.begin(), cnf._var_to_level.end(), [](unsigned level) {
        return level != NO_VAR;
      }));
    }

    // Read the clauses
    size_t clause_offset = 0;
    cnf._clause_data.reserve(nclauses * 4);
    cnf._clause_offsets.reserve(nclauses);
    while (input.good()) {
      // Comment lines may also occur among the clauses
      input >> std::ws;
      if (input.peek() == 'c') {
        input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        continue;
      }

      int literal;
      input >> literal;
      if (input.fail()) {
        if (input.eof())
          break; // this branch is taken if there is whitespace at the end of the file
        std::cerr << "error: expected an integer\n";
        return {};
      }
      if (literal == 0) {
        cnf._clause_offsets.push_back(clause_offset);
        clause_offset = cnf._clause_data.size();
      } else if (unsigned_abs(literal) > nvars) {
        std::cerr << "error: found literal " << literal << " but there are only " << nvars
                  << "variables\n";
        return {};
      } else {
        assert(literal != 0);
        cnf._clause_data.push_back(literal);
      }
    }
    // The last 0 could be omitted in the input. It could also be that the last clause is empty; in
    // that case `cnf._clause_data.size() > clause_offset` would be false.
    if (cnf._clause_data.size() > clause_offset || cnf._clause_offsets.size() == nclauses - 1) {
      cnf._clause_offsets.push_back(clause_offset);
    }

    if (cnf._clause_offsets.size() != nclauses) {
      std::cerr << "error: number of clauses does not match (" << nclauses
                << " in header, actual: " << cnf._clause_offsets.size() << ")\n";
      return {};
    }

    if (input.bad()) {
      std::cerr << "error: reading from the input file failed\n";
      return {};
    }

    return cnf;
  }

  /// Returns `true` iff there is an empty clause
  bool
  has_empty_clause() const
  {
    size_t last_offset = std::numeric_limits<size_t>::max();
    for (size_t offset : _clause_offsets) {
      if (offset == last_offset) return true;
      last_offset = offset;
    }
    return false;
  }

  /// Get the number of clauses
  unsigned
  num_clauses() const
  {
    return _clause_offsets.size();
  }

  /// Get the variable to level mapping
  const std::vector<unsigned>&
  var_to_level() const
  {
    return _var_to_level;
  }

  /// Call `f(begin, end)` for each clause, where `begin` and `end` are random access iterators over
  /// the literals of the clause. Each literal `l` is an `int` unequal to 0, where `l` refers to
  /// variable `|l| - 1`.
  template <typename F>
  void
  foreach_clause(F f) const
  {
    unsigned n = _clause_offsets.size();
    for (unsigned i = 0; i < n; ++i) {
      f(_clause_data.begin() + _clause_offsets[i],
        i == n - 1 ? _clause_data.end() : _clause_data.begin() + _clause_offsets[i + 1]);
    }
  }
};

// ========================================================================== //

/// Construct the clauses of `cnf`
///
/// This will filter out clauses equivalent to `⊤`, so the resulting vector
/// might be shorter than `cnf.num_clauses()`, unless `keep_tautologies` is set,
/// in which case they are kept as `⊤` and the clauses keep their numbering.
///
/// Precondition: there is no empty clause
template <typename Adapter>
std::vector<typename Adapter::dd_t>
construct_clauses(Adapter& adapter, const CNF& cnf, const bool keep_tautologies = false)
{
  std::vector<typename Adapter::dd_t> clauses;
  clauses.reserve(cnf.num_clauses());
  const std::vector<unsigned>& var_to_level = cnf.var_to_level();

  // polarity of variables in the current clause
  std::vector<signed char> polarities(var_to_level.size());

  // We directly construct the clauses using the builder (and do not use the
  // disjunction operator) for better performance on time-forward processing
  // implementations

  cnf.foreach_clause([&adapter, &var_to_level, &polarities, &clauses, keep_tautologies](auto begin,
                                                                                     auto end) {
    // minimum and maximum variable defined in the clause
    unsigned min_level = var_to_level.size(), max_level = 0;

    // Fill the `polarities` table
    for (; begin != end; ++begin) {
      const int literal          = *begin;
      const unsigned level       = var_to_level[unsigned_abs(literal) - 1];
      const signed char polarity = literal < 0 ? -1 : 1, prev = polarities[level];
      if (prev != 0 && prev != polarity) {
        // x ∨ ¬x ≡ ⊤, just clean the polarities and continue with the next clause
        std::fill(polarities.begin() + min_level, polarities.begin() + max_level + 1, 0);
        if (keep_tautologies) clauses.push_back(adapter.top());
        return;
      } else {
        polarities[level] = polarity;
        if (level > max_level) max_level = level;
        if (level < min_level) min_level = level;
      }
    }

    assert(min_level <= max_level); // holds by the function's precondition

    // ==============================
    // Construct the clause bottom-up
    unsigned level = var_to_level.size() - 1;

    // nodes below `max_level`
    typename Adapter::build_node_t tautology = adapter.build_node(true);
    if (Adapter::needs_extend) {
      // This is needed for ZDDs, and would not be incorrect for BDDs, still we
      // skip the unnecessary computations.
      for (; level != max_level; --level) {
        tautology = adapter.build_node(level, tautology, tautology);
      }
      assert(level == max_level);
    } else {
      level = max_level;
    }

    // nodes for `max_level`
    assert(polarities[level] == 1 || polarities[level] == -1);
    typename Adapter::build_node_t clause_build = polarities[level] == 1
      ? adapter.build_node(level, /* lo */ adapter.build_node(false), tautology)
      : adapter.build_node(level, /* lo */ tautology, adapter.build_node(false));
    if (Adapter::needs_extend && level > min_level)
      tautology = adapter.build_node(level, tautology, tautology);
    polarities[level] = 0;

    // nodes above `max_level`
    while (level-- != 0) {
      const signed char pol = polarities[level];
      if (pol == 0) {
        if (Adapter::needs_extend)
          clause_build = adapter.build_node(level, clause_build, clause_build);
      } else {
        polarities[level] = 0;
        clause_build      = pol == 1 ? adapter.build_node(level, /* lo */ clause_build, tautology)
                                     : adapter.build_node(level, /* lo */ tautology, clause_build);
      }
      if (Adapter::needs_extend && level > min_level)
        tautology = adapter.build_node(level, tautology, tautology);
    }

    clauses.push_back(adapter.build());
  });

  return clauses;
}

/// Whether the operations of `Adapter` generate a clausal proof, which is
/// initialised with the input clauses and gives the clauses by their numbers
template <typename Adapter, typename = void>
struct is_proof_generating : std::false_type
{};

template <typename Adapter>
struct is_proof_generating<Adapter, std::void_t<decltype(Adapter::proof_generating)>>
  : std::bool_constant<Adapter::proof_generating>
{};

/// Initialise the proof of a proof-generating `adapter` with the clauses and
/// the variable order of `cnf`, written to `path`
///
/// Returns `false` on failure.
template <typename Adapter>
bool
init_proof(Adapter& adapter, const CNF& cnf, const std::string& path)
{
  std::vector<std::vector<int>> clauses;
  clauses.reserve(cnf.num_clauses());
  cnf.foreach_clause([&clauses](auto begin, auto end) { clauses.emplace_back(begin, end); });

  const std::vector<unsigned>& var_to_level = cnf.var_to_level();
  std::vector<int> level_to_var(var_to_level.size());
  for (size_t x = 0; x < var_to_level.size(); ++x) level_to_var[var_to_level[x]] = x + 1;

  return adapter.init_proof(clauses, level_to_var, path);
}

/// The clauses of `cnf` as premises of the proof of a proof-generating
/// `adapter`, all kept and in their input numbering
template <typename Adapter>
std::vector<typename Adapter::dd_t>
premises(Adapter& adapter, const CNF& cnf)
{
  std::vector<typename Adapter::dd_t> clauses;
  clauses.reserve(cnf.num_clauses());
  for (unsigned id = 1; id <= cnf.num_clauses(); ++id) clauses.push_back(adapter.clause(id));
  return clauses;
}

/// Conjoin the clauses with an (approximately) balanced bracketing
/// `(c0 ∧ c1) ∧ (c2 ∧ (c3 ∧ c4))`
///
/// In the paper [Configuring BDD Compilation Techniques for Feature
/// Models](https://doi.org/10.1145/3646548.3676538), this was shown to be much
/// better than a left-deep clause bracketing `((c1 ∧ c2) ∧ c3) ∧ c4`.
///
/// Importantly, this function does not commute any operands, i.e., we do not
/// conjoin `c0` and `c4` (or any dependant intermediate results) before `c1`
/// has been processed.
template <typename Adapter, typename IT>
typename Adapter::dd_t
conjoin(Adapter& adapter, const IT begin, const IT end)
{
  if (begin == end) return adapter.top();
  auto d = std::distance(begin, end);
  if (d == 1) return *begin;

  const IT mid               = begin + d / 2;
  typename Adapter::dd_t res = conjoin(adapter, begin, mid) & conjoin(adapter, mid, end);

#ifdef BDD_BENCHMARK_STATS
  const size_t nodecount = adapter.nodecount(res);
  largest_bdd            = std::max(largest_bdd, nodecount);
  total_nodes += nodecount;
#endif // BDD_BENCHMARK_STATS

  return res;
}

// ========================================================================== //
// Schedules and bucket elimination, after the SAT solver `tbsat` of TBUDDY
// (https://github.com/rebryant/tbuddy-artifact, `src/tbsat/teval.cpp`)

/// A command of a schedule file
///
/// A schedule is a stack program over terms, each the BDD of a set of clauses
/// with some variables existentially quantified. The line `c i j ...` pushes
/// the clauses numbered `i`, `j`, ... (from 1). The line `a k` pops a term and
/// conjoins it with the `k` terms below it, one after the other, and pushes the
/// result. The line `q x y ...` replaces the top term by its existential
/// quantification over the variables `x`, `y`, ... (from 1). Lines starting
/// with `#` or `i` are comments. The commands `=` and `g` of `tbsat`, which
/// introduce and eliminate parity constraints, are not supported.
struct schedule_command
{
  /// One of `c`, `a` and `q`
  char op;

  /// The clauses, the number of conjunctions, or the variables
  std::vector<int> args;
};

/// Parse the schedule file at `path` for a CNF with `nclauses` clauses over
/// `nvars` variables
///
/// The schedule must push each clause at most once and never pop more terms
/// than the stack holds.
std::optional<std::vector<schedule_command>>
parse_schedule(const std::string& path, const size_t nclauses, const size_t nvars)
{
  std::ifstream input(path, std::ios::in);
  std::vector<schedule_command> commands;
  std::vector<bool> pushed(nclauses, false);
  size_t height = 0;
  std::string text;
  unsigned line = 0;
  while (std::getline(input, text)) {
    ++line;
    std::istringstream words(text);
    char op;
    if (!(words >> op) || op == '#' || op == 'i') continue;
    if (op != 'c' && op != 'a' && op != 'q') {
      std::cerr << "error: unsupported schedule command '" << op << "' (line " << line << ")\n";
      return {};
    }

    schedule_command command{ op, {} };
    int arg;
    while (words >> arg) command.args.push_back(arg);
    if (!words.eof()) {
      std::cerr << "error: expected an integer (schedule line " << line << ")\n";
      return {};
    }

    const size_t bound = op == 'c' ? nclauses : nvars;
    if (op == 'a' ? command.args.size() != 1 || command.args[0] < 1
                  : std::any_of(command.args.begin(), command.args.end(), [bound](int a) {
                      return a < 1 || size_t(a) > bound;
                    })) {
      std::cerr << "error: invalid arguments of '" << op << "' (schedule line " << line << ")\n";
      return {};
    }

    // Track the stack
    bool valid = true;
    switch (op) {
    case 'c':
      for (const int i : command.args) {
        valid = valid && !pushed[i - 1];
        pushed[i - 1] = true;
      }
      height += command.args.size();
      break;
    case 'a':
      valid = size_t(command.args[0]) < height;
      height -= valid ? command.args[0] : 0;
      break;
    case 'q': valid = height > 0; break;
    }
    if (!valid) {
      std::cerr << "error: '" << op << "' reuses a clause or pops an empty stack (schedule line "
                << line << ")\n";
      return {};
    }
    commands.push_back(std::move(command));
  }
  if (input.bad()) {
    std::cerr << "error: reading from the schedule file failed\n";
    return {};
  }
  return commands;
}

/// The operations of a run that conjoins and quantifies
template <typename Adapter>
class reduction
{
  using dd_t = typename Adapter::dd_t;

  Adapter& _adapter;

public:
  /// Number of conjunctions computed
  size_t conjunctions = 0;

  /// Number of existential quantifications computed
  size_t quantifications = 0;

  reduction(Adapter& adapter)
    : _adapter(adapter)
  {}

  dd_t
  conjoin(const dd_t& f, const dd_t& g)
  {
    ++conjunctions;
    dd_t res = f & g;

#ifdef BDD_BENCHMARK_STATS
    const size_t nodecount = _adapter.nodecount(res);
    largest_bdd            = std::max(largest_bdd, nodecount);
    total_nodes += nodecount;
#endif // BDD_BENCHMARK_STATS

    return res;
  }

  /// Quantify `f` over the given levels, sorted in ascending order
  dd_t
  exists(const dd_t& f, const std::vector<int>& levels)
  {
    ++quantifications;
    return _adapter.exists(f, levels.crbegin(), levels.crend());
  }

  /// Level of the root of `f`, which must not be constant
  ///
  /// Every path from the root tests the root's variable, so it is the least
  /// variable of the path that `pickcube` returns.
  int
  root_level(const dd_t& f)
  {
    const auto path = _adapter.pickcube(f);
    assert(!path.empty());
    int level = path.front().first;
    for (const auto& literal : path) level = std::min<int>(level, literal.first);
    return level;
  }

  /// Bucket elimination over `terms`, whose variables are among the first
  /// `nlevels` levels
  ///
  /// A term is kept in the bucket of its root's level. The buckets are
  /// processed by increasing level: the terms of a bucket are conjoined in
  /// pairs, in the order they were added, and each product is added to the
  /// bucket of its root's level, which may be the current one. The last term of
  /// a bucket is quantified over the bucket's variable and added to the bucket
  /// of its root's level unless it is `⊤`. The result is `⊥` as soon as a term
  /// is `⊥`, and `⊤` once all buckets are empty.
  dd_t
  bucket_eliminate(std::vector<dd_t> terms, const size_t nlevels)
  {
    std::vector<std::vector<dd_t>> buckets(nlevels);
    for (dd_t& f : terms) {
      if (f == _adapter.bot()) return f;
      if (f == _adapter.top()) continue;
      buckets[root_level(f)].push_back(std::move(f));
    }
    terms.clear();

    for (size_t level = 0; level < nlevels; ++level) {
      size_t next = 0;
      while (next + 1 < buckets[level].size()) {
        const dd_t f = std::move(buckets[level][next++]);
        const dd_t g = std::move(buckets[level][next++]);
        dd_t h       = conjoin(f, g);
        if (h == _adapter.bot()) return h;
        buckets[root_level(h)].push_back(std::move(h));
      }
      if (next + 1 == buckets[level].size()) {
        dd_t h = exists(buckets[level][next], { int(level) });
        if (h != _adapter.top()) buckets[root_level(h)].push_back(std::move(h));
      }
      buckets[level] = {};
    }
    return _adapter.top();
  }

  /// Run the schedule `commands`, as checked by `parse_schedule`, over
  /// `clauses`, where `var_to_level` maps the variables to levels
  ///
  /// The result is `⊥` as soon as a conjunction is `⊥`. If the schedule leaves
  /// more than one term on the stack, the remaining terms are combined by
  /// bucket elimination.
  dd_t
  run_schedule(std::vector<dd_t> clauses,
               const std::vector<schedule_command>& commands,
               const std::vector<unsigned>& var_to_level)
  {
    std::vector<dd_t> stack;
    std::vector<int> levels;
    for (const schedule_command& command : commands) {
      switch (command.op) {
      case 'c':
        for (const int i : command.args) stack.push_back(std::move(clauses[i - 1]));
        break;

      case 'a': {
        const size_t k = command.args[0];
        assert(k < stack.size());
        dd_t product = std::move(stack.back());
        stack.pop_back();
        for (size_t j = 0; j < k; ++j) {
          product = conjoin(product, stack.back());
          stack.pop_back();
          if (product == _adapter.bot()) return product;
        }
        stack.push_back(std::move(product));
        break;
      }

      case 'q':
        assert(!stack.empty());
        levels.clear();
        for (const int x : command.args) levels.push_back(var_to_level[x - 1]);
        std::sort(levels.begin(), levels.end());
        stack.back() = exists(stack.back(), levels);
        break;
      }
    }
    if (stack.size() == 1) return stack.back();
    return bucket_eliminate(std::move(stack), var_to_level.size());
  }
};

// ========================================================================== //
template <typename Adapter>
int
run_cnf(int argc, char** argv)
{
  bool should_exit = parse_input<parsing_policy>(argc, argv);
  if (should_exit) { return -1; }

  if (file.empty()) {
    std::cerr << "Input file not specified\n";
    return -1;
  }

  const bool quantifying = !schedule_file.empty() || bucket_elimination;
  if (!schedule_file.empty() && bucket_elimination) {
    std::cerr << "A schedule and bucket elimination exclude each other\n";
    return -1;
  }
  if (quantifying && satcount) {
    std::cerr << "Quantifying runs decide satisfiability and do not count\n";
    return -1;
  }
  if (quantifying && Adapter::needs_extend) {
    std::cerr << "Quantifying runs need a BDD package\n";
    return -1;
  }
  constexpr bool proving = is_proof_generating<Adapter>::value;
  if (proving && proof_file.empty()) {
    std::cerr << "This package generates a proof, whose path must be given (-p)\n";
    return -1;
  }
  if (!proving && !proof_file.empty()) {
    std::cerr << "This package does not generate a proof\n";
    return -1;
  }
  if (proving && satcount) {
    std::cerr << "The proof does not certify a model count\n";
    return -1;
  }

  // Read the file
  std::optional<CNF> cnf = CNF::parse_dimacs_cnf(file);
  if (!cnf) {
    return -1; // Parser error has been printed
  }
  if (cnf->has_empty_clause()) {
    std::cerr << "The CNF contains an empty clause and is thus trivially unsatisfiable\n";
    return -1;
  }

  std::optional<std::vector<schedule_command>> schedule;
  if (!schedule_file.empty()) {
    schedule = parse_schedule(schedule_file, cnf->num_clauses(), cnf->var_to_level().size());
    if (!schedule) {
      return -1; // Parser error has been printed
    }
  }

  // =========================================================================
  // Initialise BDD manager
  return run<Adapter>("cnf", cnf->var_to_level().size(), [&](Adapter& adapter) {
    uint64_t solutions;

    // ========================================================================
    // Initialise the proof with the input clauses

    time_duration proof_init_time = 0;
    if constexpr (proving) {
      const time_point t0 = now();
      if (!init_proof(adapter, *cnf, proof_file)) { return -1; }
      proof_init_time = duration_ms(t0, now());
    }

    // ========================================================================
    // Construct a BDD for each clause

    std::cout << json::field("clauses") << json::brace_open << json::endl << json::flush;

    const time_point t1 = now();
    std::vector<typename Adapter::dd_t> clauses;
    if constexpr (proving) {
      clauses = premises(adapter, *cnf);
    } else {
      clauses = construct_clauses(adapter, *cnf, quantifying);
    }
    const time_point t2 = now();

    const time_duration clause_cons_time = duration_ms(t1, t2);
    std::cout << json::field("amount") << json::value(clauses.size()) << json::comma << json::endl;
    std::cout << json::field("time (ms)") << json::value(clause_cons_time) << json::endl;

    std::cout << json::brace_close << json::comma << json::endl;

    // ========================================================================
    // Compute conjunction
    std::cout << json::field("apply") << json::brace_open << json::endl << json::flush;

#ifdef BDD_BENCHMARK_STATS
    std::cout << json::field("intermediate results") << json::brace_open << json::endl;
#endif // BDD_BENCHMARK_STATS

    // Quantifying runs are rejected for ZDDs above, whose adapters lack some of
    // the operations they need
    reduction<Adapter> ops(adapter);
    typename Adapter::dd_t res = adapter.bot();
    const time_point t3        = now();
    if constexpr (!Adapter::needs_extend) {
      res = schedule ? ops.run_schedule(std::move(clauses), *schedule, cnf->var_to_level())
        : bucket_elimination ? ops.bucket_eliminate(std::move(clauses), cnf->var_to_level().size())
                             : conjoin(adapter, clauses.cbegin(), clauses.cend());
    } else {
      res = conjoin(adapter, clauses.cbegin(), clauses.cend());
    }
    const time_point t4 = now();

    const time_duration apply_time = duration_ms(t3, t4);

#ifdef BDD_BENCHMARK_STATS
    std::cout << json::field("total processed (nodes)") << json::value(total_nodes) << json::comma
              << json::endl;
    std::cout << json::field("largest size (nodes)") << json::value(largest_bdd) << json::endl;
    std::cout << json::brace_close << json::comma << json::endl;
#endif // BDD_BENCHMARK_STATS
    if (quantifying) {
      const std::string_view method = schedule ? "schedule" : "bucket elimination";
      std::cout << json::field("method") << json::value(method) << json::comma << json::endl;
      std::cout << json::field("conjunctions") << json::value(ops.conjunctions) << json::comma
                << json::endl;
      std::cout << json::field("quantifications") << json::value(ops.quantifications)
                << json::comma << json::endl;
    }
    std::cout << json::field("final size (nodes)") << json::value(adapter.nodecount(res))
              << json::comma << json::endl;
    std::cout << json::field("time (ms)") << json::value(apply_time) << json::endl;
    std::cout << json::brace_close << json::comma << json::endl << json::flush;

    if (quantifying) {
      const bool satisfiable = res != adapter.bot();
      std::cout << json::field("satisfiable") << json::value(satisfiable) << json::comma
                << json::endl;
    }

    if constexpr (proving) {
      std::cout << json::field("proof") << json::brace_open << json::endl;
      std::cout << json::field("init time (ms)") << json::value(proof_init_time) << json::comma
                << json::endl;
      std::cout << json::field("clauses") << json::value(adapter.proof_clauses()) << json::comma
                << json::endl;
      std::cout << json::field("refutation") << json::value(res == adapter.bot()) << json::endl;
      std::cout << json::brace_close << json::comma << json::endl << json::flush;
    }

    // ========================================================================
    // Count number of solutions
    time_duration counting_time = 0;
    if (satcount) {
      std::cout << json::field("satcount") << json::brace_open << json::endl << json::flush;

      const time_point t5 = now();
      solutions           = adapter.satcount(res);
      const time_point t6 = now();

      counting_time = duration_ms(t5, t6);

      std::cout << json::field("result") << json::value(solutions) << json::comma << json::endl;
      std::cout << json::field("time (ms)") << json::value(counting_time) << json::endl;
      std::cout << json::brace_close << json::comma << json::endl << json::flush;
    }

    // ========================================================================
    std::cout << json::field("total time (ms)")
              << json::value(init_time + proof_init_time + clause_cons_time + apply_time
                             + counting_time)
              << json::endl
              << json::flush;

    return 0;
  });
}
