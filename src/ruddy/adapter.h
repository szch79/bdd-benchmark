#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "../common/adapter.h"

#include "ruddy.h"

////////////////////////////////////////////////////////////////////////////////
/// Memory of Ruddy. An entry of the node table holds a node (variable, low and
/// high child) and three links of the table's parent trie, all of the width of
/// the table's identifiers. The table uses 16-bit identifiers below 2^16
/// entries and 64-bit identifiers only beyond 2^32 entries, so every table
/// that '-M' can bound in practice has 32-bit identifiers and 6*4 = 24 bytes
/// per entry. The task caches of the operations are not counted, as in the
/// LibBDD adapter.
////////////////////////////////////////////////////////////////////////////////

/// Size of an entry of the node table in Ruddy
constexpr size_t sizeof_slot = 24u;

namespace ruddy
{
  class bdd_function;

  /// \brief A manager of the shared representation, which owns the node table.
  ///
  /// The BDDs of a manager point to it, so it must outlive them.  Releasing a BDD does not touch
  /// the manager, so the BDDs may also be released after it.
  class manager
  {
    capi::ruddy_manager_t _manager;

  public:
    manager(size_t max_slots) noexcept
      : _manager(capi::ruddy_manager_new(max_slots))
    {}

    manager(const manager& other) = delete;

    manager&
    operator=(const manager& other) = delete;

    ~manager() noexcept
    {
      capi::ruddy_manager_free(_manager);
    }

    size_t
    node_count() const noexcept
    {
      return capi::ruddy_manager_node_count(_manager);
    }

    bdd_function
    ithvar(uint32_t var) const noexcept;
    bdd_function
    nithvar(uint32_t var) const noexcept;

    bdd_function
    top() const noexcept;
    bdd_function
    bot() const noexcept;

    bdd_function
    if_then_else(uint32_t var, const bdd_function& t, const bdd_function& e) const noexcept;
  };

  /// \brief A BDD of a manager, owning one handle of the C interface.
  ///
  /// A handle holds a reference to the root of the BDD, which keeps its nodes alive across garbage
  /// collections, so copying a BDD takes a new handle and destroying one releases it.
  class bdd_function
  {
    capi::ruddy_bdd_t _func;

    friend class manager;

    bdd_function(capi::ruddy_bdd_t func) noexcept
      : _func(func)
    {}

  public:
    bdd_function() noexcept
      : _func({ ._p = nullptr })
    {}

    bdd_function(const bdd_function& other) noexcept
      : _func(other._func._p ? capi::ruddy_bdd_clone(other._func) : other._func)
    {}

    bdd_function(bdd_function&& other) noexcept
      : _func(other._func)
    {
      other._func._p = nullptr;
    }

    ~bdd_function() noexcept
    {
      capi::ruddy_bdd_free(_func);
    }

    bdd_function&
    operator=(const bdd_function& rhs) noexcept
    {
      if (this != &rhs) {
        capi::ruddy_bdd_free(_func);
        _func = rhs._func._p ? capi::ruddy_bdd_clone(rhs._func) : rhs._func;
      }
      return *this;
    }

    bdd_function&
    operator=(bdd_function&& rhs) noexcept
    {
      if (this != &rhs) {
        capi::ruddy_bdd_free(_func);
        _func        = rhs._func;
        rhs._func._p = nullptr;
      }
      return *this;
    }

    bool
    is_invalid() const noexcept
    {
      return _func._p == nullptr;
    }

    bool
    operator==(const bdd_function& rhs) const noexcept
    {
      return (_func._p && rhs._func._p && capi::ruddy_bdd_eq(_func, rhs._func))
        || (!_func._p && !rhs._func._p);
    }

    bool
    operator!=(const bdd_function& rhs) const noexcept
    {
      return !(*this == rhs);
    }

    bdd_function
    and_not(const bdd_function& rhs) const noexcept
    {
      assert(_func._p && rhs._func._p);
      return capi::ruddy_and_not(_func, rhs._func);
    }

    bdd_function
    operator~() const noexcept
    {
      assert(_func._p);
      return capi::ruddy_not(_func);
    }

    bdd_function
    operator&(const bdd_function& rhs) const noexcept
    {
      assert(_func._p && rhs._func._p);
      return capi::ruddy_and(_func, rhs._func);
    }

    bdd_function&
    operator&=(const bdd_function& rhs) noexcept
    {
      return (*this = *this & rhs);
    }

    bdd_function
    operator|(const bdd_function& rhs) const noexcept
    {
      assert(_func._p && rhs._func._p);
      return capi::ruddy_or(_func, rhs._func);
    }

    bdd_function&
    operator|=(const bdd_function& rhs) noexcept
    {
      return (*this = *this | rhs);
    }

    bdd_function
    operator^(const bdd_function& rhs) const noexcept
    {
      assert(_func._p && rhs._func._p);
      return capi::ruddy_xor(_func, rhs._func);
    }

    bdd_function&
    operator^=(const bdd_function& rhs) noexcept
    {
      return (*this = *this ^ rhs);
    }

    bdd_function
    operator-(const bdd_function& rhs) const noexcept
    {
      return this->and_not(rhs);
    }

    bdd_function&
    operator-=(const bdd_function& rhs) noexcept
    {
      return (*this = *this - rhs);
    }

    bdd_function
    imp(const bdd_function& rhs) const noexcept
    {
      assert(_func._p && rhs._func._p);
      return capi::ruddy_imp(_func, rhs._func);
    }

    bdd_function
    iff(const bdd_function& rhs) const noexcept
    {
      assert(_func._p && rhs._func._p);
      return capi::ruddy_iff(_func, rhs._func);
    }

    bdd_function
    exists(const uint32_t* vars, size_t num_vars) const noexcept
    {
      assert(_func._p);
      return capi::ruddy_exists(_func, vars, num_vars);
    }

    bdd_function
    forall(const uint32_t* vars, size_t num_vars) const noexcept
    {
      assert(_func._p);
      return capi::ruddy_forall(_func, vars, num_vars);
    }

    /// \brief The number of nodes reachable from the BDD, counting both sinks unless the BDD is
    ///        constant.
    uint64_t
    node_count() const noexcept
    {
      assert(_func._p);
      return capi::ruddy_node_count(_func);
    }

    /// \brief The number of models over the variables '0' to 'num_vars - 1'.
    double
    sat_count(uint32_t num_vars) const noexcept
    {
      assert(_func._p);
      return capi::ruddy_sat_count(_func, num_vars);
    }

    /// \brief Writes the first satisfying path to 'out', one entry for each of the variables '0'
    ///        to 'largest', and returns false when the BDD is false.
    bool
    pick_path(uint32_t largest, int8_t* out) const noexcept
    {
      assert(_func._p);
      return capi::ruddy_pick_path(_func, largest, out);
    }
  };

  inline bdd_function
  manager::ithvar(uint32_t var) const noexcept
  {
    return capi::ruddy_literal(_manager, var, true);
  }

  inline bdd_function
  manager::nithvar(uint32_t var) const noexcept
  {
    return capi::ruddy_literal(_manager, var, false);
  }

  inline bdd_function
  manager::top() const noexcept
  {
    return capi::ruddy_true(_manager);
  }

  inline bdd_function
  manager::bot() const noexcept
  {
    return capi::ruddy_false(_manager);
  }

  /// \brief The node on 'var' with children 't' and 'e', both of which must be over variables
  ///        greater than 'var'.
  inline bdd_function
  manager::if_then_else(uint32_t var, const bdd_function& t, const bdd_function& e) const noexcept
  {
    assert(t._func._p && e._func._p);
    return capi::ruddy_if_then_else(_manager, var, t._func, e._func);
  }
} // namespace ruddy

class ruddy_bdd_adapter
{
public:
  static constexpr std::string_view name = "Ruddy";
  static constexpr std::string_view dd   = "BDD";

  static constexpr bool needs_extend     = false;
  static constexpr bool needs_frame_rule = true;

  static constexpr bool complement_edges = false;

  using dd_t         = ruddy::bdd_function;
  using build_node_t = ruddy::bdd_function;

private:
  const int _varcount;
  ruddy::manager _manager;
  ruddy::bdd_function _latest_build;

  // Init and Deinit
public:
  /// \brief A manager with automatic garbage collection.
  ///
  /// The manager grows its node table on demand.  The C interface bounds it by the entries that
  /// fit into '-M' MiB and aborts when an operation leaves more, even after a collection.  Its
  /// node table starts with 16-bit identifiers, on which a quantified variable above 8190
  /// would be taken for another, so the variables are limited to that range.
  ruddy_bdd_adapter(int varcount)
    : _varcount(varcount)
    , _manager(static_cast<size_t>(M) * 1024 * 1024 / sizeof_slot)
  {
    if (varcount < 1 || 8191 < varcount) {
      throw std::invalid_argument("ruddy: the number of variables must be between 1 and 8191");
    }
  }

  template <typename F>
  int
  run(const F& f)
  {
    return f();
  }

  // BDD Operations
public:
  inline ruddy::bdd_function
  top()
  {
    return _manager.top();
  }

  inline ruddy::bdd_function
  bot()
  {
    return _manager.bot();
  }

  inline ruddy::bdd_function
  ithvar(uint32_t label)
  {
    return _manager.ithvar(label);
  }

  inline ruddy::bdd_function
  nithvar(uint32_t label)
  {
    return _manager.nithvar(label);
  }

  /// \brief The conjunction of the variables of a range, in any order and with repetitions.
  ///
  /// The cube is built from the sink upwards by the node constructor, which needs the variables
  /// in decreasing order and without repetitions.
  template <typename IT>
  inline ruddy::bdd_function
  cube(IT rbegin, IT rend)
  {
    std::vector<uint32_t> vars(rbegin, rend);
    std::sort(vars.begin(), vars.end(), std::greater<uint32_t>());
    vars.erase(std::unique(vars.begin(), vars.end()), vars.end());

    ruddy::bdd_function res = top();
    for (const uint32_t x : vars) { res = make_node(x, bot(), res); }
    return res;
  }

  /// \brief The conjunction of the variables that satisfy a predicate.
  inline ruddy::bdd_function
  cube(const std::function<bool(int)>& pred)
  {
    ruddy::bdd_function res = top();
    for (int x = _varcount - 1; 0 <= x; --x) {
      if (pred(x)) { res = make_node(x, bot(), res); }
    }
    return res;
  }

  inline ruddy::bdd_function
  apply_and(const ruddy::bdd_function& f, const ruddy::bdd_function& g)
  {
    return f & g;
  }

  inline ruddy::bdd_function
  apply_or(const ruddy::bdd_function& f, const ruddy::bdd_function& g)
  {
    return f | g;
  }

  inline ruddy::bdd_function
  apply_diff(const ruddy::bdd_function& f, const ruddy::bdd_function& g)
  {
    return f - g;
  }

  inline ruddy::bdd_function
  apply_imp(const ruddy::bdd_function& f, const ruddy::bdd_function& g)
  {
    return f.imp(g);
  }

  inline ruddy::bdd_function
  apply_xor(const ruddy::bdd_function& f, const ruddy::bdd_function& g)
  {
    return f ^ g;
  }

  inline ruddy::bdd_function
  apply_xnor(const ruddy::bdd_function& f, const ruddy::bdd_function& g)
  {
    return f.iff(g);
  }

  /// \brief The if-then-else of three BDDs, as the disjunction of 'i & t' and 'e - i'.
  ///
  /// The library has no if-then-else on three BDDs, only the node constructor on a variable, and
  /// negation copies the whole diagram, so the difference stands in for the conjunction with the
  /// negated condition.
  inline ruddy::bdd_function
  ite(const ruddy::bdd_function& i, const ruddy::bdd_function& t, const ruddy::bdd_function& e)
  {
    return (i & t) | (e - i);
  }

  template <typename IT>
  inline ruddy::bdd_function
  extend(const ruddy::bdd_function& f, IT /*begin*/, IT /*end*/)
  {
    return f;
  }

  inline ruddy::bdd_function
  exists(const ruddy::bdd_function& f, int i)
  {
    const uint32_t var = i;
    return f.exists(&var, 1);
  }

  inline ruddy::bdd_function
  exists(const ruddy::bdd_function& f, const std::function<bool(int)>& pred)
  {
    const std::vector<uint32_t> vars = variables(pred);
    return f.exists(vars.data(), vars.size());
  }

  template <typename IT>
  inline ruddy::bdd_function
  exists(const ruddy::bdd_function& f, IT rbegin, IT rend)
  {
    const std::vector<uint32_t> vars(rbegin, rend);
    return f.exists(vars.data(), vars.size());
  }

  inline ruddy::bdd_function
  forall(const ruddy::bdd_function& f, int i)
  {
    const uint32_t var = i;
    return f.forall(&var, 1);
  }

  inline ruddy::bdd_function
  forall(const ruddy::bdd_function& f, const std::function<bool(int)>& pred)
  {
    const std::vector<uint32_t> vars = variables(pred);
    return f.forall(vars.data(), vars.size());
  }

  template <typename IT>
  inline ruddy::bdd_function
  forall(const ruddy::bdd_function& f, IT rbegin, IT rend)
  {
    const std::vector<uint32_t> vars(rbegin, rend);
    return f.forall(vars.data(), vars.size());
  }

  inline ruddy::bdd_function
  relnext(const ruddy::bdd_function&, const ruddy::bdd_function&, const ruddy::bdd_function&)
  {
    throw std::logic_error("ruddy: variable renaming is not supported");
  }

  inline ruddy::bdd_function
  relprev(const ruddy::bdd_function&, const ruddy::bdd_function&, const ruddy::bdd_function&)
  {
    throw std::logic_error("ruddy: variable renaming is not supported");
  }

  /// \brief The number of nodes reachable from f, both sinks included, as BuDDy's adapter counts
  ///        them.
  ///
  /// The library counts both sinks for every BDD but false, so true, the only BDD with a count of
  /// two, is mapped to one.
  inline uint64_t
  nodecount(const ruddy::bdd_function& f)
  {
    const uint64_t c = f.node_count();
    return c == 2 ? 1 : c;
  }

  inline uint64_t
  satcount(const ruddy::bdd_function& f)
  {
    return this->satcount(f, this->_varcount);
  }

  /// \brief The number of models of f over 'vc' of the variables, on which f depends only.
  ///
  /// The library counts models as doubles over the variables up to a given one, which overflows
  /// for a function over few of many variables, so the C interface counts over 'vc' variables as
  /// CUDD does, by the fraction of satisfying assignments.
  inline uint64_t
  satcount(const ruddy::bdd_function& f, const size_t vc)
  {
    assert(vc <= static_cast<size_t>(this->_varcount));

    return checked_satcount(f.sat_count(vc));
  }

  /// \brief The cube of the literals of the path of 'pickcube', false when f is false.
  inline ruddy::bdd_function
  satone(const ruddy::bdd_function& f)
  {
    const std::vector<int8_t> path = pick_path(f);
    if (path.empty()) { return bot(); }

    ruddy::bdd_function res = top();
    for (int x = _varcount - 1; 0 <= x; --x) {
      if (path[x] == 1) { res = make_node(x, bot(), res); }
      if (path[x] == 0) { res = make_node(x, res, bot()); }
    }
    return res;
  }

  inline ruddy::bdd_function
  satone(const ruddy::bdd_function&, const ruddy::bdd_function&)
  {
    throw std::logic_error("ruddy: a satisfying cube over a set of variables is not supported");
  }

  /// \brief The literals of the first satisfying path of the library, by increasing variable.
  ///
  /// The library enumerates the paths depth first, taking the low child before the high one.
  inline std::vector<std::pair<uint32_t, char>>
  pickcube(const ruddy::bdd_function& f)
  {
    const std::vector<int8_t> path = pick_path(f);

    std::vector<std::pair<uint32_t, char>> res;
    for (size_t x = 0; x < path.size(); ++x) {
      if (path[x] < 0) { continue; }
      res.emplace_back(x, static_cast<char>('0' + path[x]));
    }
    return res;
  }

  void
  print_dot(const ruddy::bdd_function&, const std::string&)
  {
    std::cerr << "ruddy_bdd_adapter does not support dot export" << std::endl;
  }

  void
  save(const ruddy::bdd_function&, const std::string&)
  {
    std::cerr << "ruddy_bdd_adapter does not support BDD export" << std::endl;
  }

  // BDD Build Operations
public:
  inline ruddy::bdd_function
  build_node(const bool value)
  {
    ruddy::bdd_function res = value ? top() : bot();
    if (_latest_build.is_invalid() || _latest_build == top() || _latest_build == bot()) {
      _latest_build = res;
    }
    return res;
  }

  /// \brief The decision on 'label' with the given children.
  ///
  /// The drivers build a diagram from its sinks upwards, so both children are over variables
  /// greater than 'label', which is the precondition of the node constructor.
  inline ruddy::bdd_function
  build_node(const uint32_t label, const ruddy::bdd_function& low, const ruddy::bdd_function& high)
  {
    return _latest_build = make_node(label, low, high);
  }

  inline ruddy::bdd_function
  build()
  {
    return std::move(_latest_build);
  }

  // Statistics
public:
  /// \brief The number of nodes in the manager's table, including those not yet collected.
  inline size_t
  allocated_nodes()
  {
    return _manager.node_count();
  }

  void
  print_stats()
  {}

private:
  inline ruddy::bdd_function
  make_node(const uint32_t label, const ruddy::bdd_function& low, const ruddy::bdd_function& high)
  {
    return _manager.if_then_else(label, high, low);
  }

  inline std::vector<uint32_t>
  variables(const std::function<bool(int)>& pred)
  {
    std::vector<uint32_t> vars;
    for (int x = 0; x < _varcount; ++x) {
      if (pred(x)) { vars.push_back(x); }
    }
    return vars;
  }

  /// \brief The first satisfying path of f, one entry per variable, empty when f is false.
  inline std::vector<int8_t>
  pick_path(const ruddy::bdd_function& f)
  {
    std::vector<int8_t> path(_varcount);
    if (!f.pick_path(_varcount - 1, path.data())) { path.clear(); }
    return path;
  }
};
