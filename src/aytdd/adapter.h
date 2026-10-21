#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../common/adapter.h"

#include <aytdd/bcdd_mc.h>

namespace aytdd
{
  /// \brief The one manager of the process.
  ///
  /// Every edge of the C interface is a bare word that is meaningful only with the manager that
  /// produced it, and each benchmark builds exactly one manager, so the adapter keeps it here
  /// rather than inside every BDD.
  inline aytdd_bcdd_manager* mgr = nullptr;

  /// \brief A BDD of that manager, with the operators the drivers use.
  ///
  /// The package does not reclaim nodes, so a BDD owns nothing and copying one is copying a word.
  struct bcdd_function
  {
    aytdd_bcdd f = aytdd_bcdd_bot();

    bcdd_function() = default;

    bcdd_function(aytdd_bcdd f)
      : f(f)
    {}

    bool
    operator==(const bcdd_function& o) const
    {
      return f == o.f;
    }

    bool
    operator!=(const bcdd_function& o) const
    {
      return f != o.f;
    }

    bcdd_function
    operator~() const
    {
      return aytdd_bcdd_not(f);
    }

    bcdd_function
    operator&(const bcdd_function& o) const
    {
      return aytdd_bcdd_and(mgr, f, o.f);
    }

    bcdd_function
    operator|(const bcdd_function& o) const
    {
      return aytdd_bcdd_or(mgr, f, o.f);
    }

    bcdd_function
    operator^(const bcdd_function& o) const
    {
      return aytdd_bcdd_xor(mgr, f, o.f);
    }

    bcdd_function
    operator-(const bcdd_function& o) const
    {
      return aytdd_bcdd_diff(mgr, f, o.f);
    }

    bcdd_function&
    operator&=(const bcdd_function& o)
    {
      return *this = *this & o;
    }

    bcdd_function&
    operator|=(const bcdd_function& o)
    {
      return *this = *this | o;
    }

    bcdd_function&
    operator^=(const bcdd_function& o)
    {
      return *this = *this ^ o;
    }

    bcdd_function&
    operator-=(const bcdd_function& o)
    {
      return *this = *this - o;
    }
  };
} // namespace aytdd

// The name of the package in the output, which a build over another node table of Aytdd sets.
#ifndef AYTDD_NAME
#define AYTDD_NAME "Aytdd"
#endif

class aytdd_bcdd_adapter
{
public:
  static constexpr std::string_view name = AYTDD_NAME;
  static constexpr std::string_view dd   = "BCDD";

  static constexpr bool needs_extend     = false;
  static constexpr bool needs_frame_rule = true;

  static constexpr bool complement_edges = true;

  using dd_t         = aytdd::bcdd_function;
  using build_node_t = aytdd::bcdd_function;

private:
  const int _varcount;
  aytdd::bcdd_function _latest_build;

  aytdd::bcdd_function _vars_relnext;
  aytdd_bcdd_subst* _subst_relnext = nullptr;

  aytdd::bcdd_function _vars_relprev;
  aytdd_bcdd_subst* _subst_relprev = nullptr;

  // Init and Deinit
public:
  aytdd_bcdd_adapter(int varcount)
    : _varcount(varcount)
  {
    // The package never reclaims nodes.  Its node table grows beyond the reservation made here at
    // roughly 24 bytes per decision, while each computed table keeps a fixed number of 16-byte
    // slots, rounded down to a power of two.  As in the BuDDy adapter, the conjunction and
    // relational-product tables take one slot per 64 decisions each, and the less frequent
    // exclusive-disjunction and if-then-else tables one per 256 each.  The node table takes the
    // rest of '-M', at 24 + 16 * (2/64 + 2/256) = 197/8 bytes per decision.
    const size_t bytes = static_cast<size_t>(M) * 1024 * 1024;
    const size_t nodes = bytes * 8 / 197;
    aytdd_bcdd_config config{};
    config.node_table_size      = nodes;
    config.and_cache_size       = nodes / 64;
    config.xor_cache_size       = nodes / 256;
    config.ite_cache_size       = nodes / 256;
    config.and_exist_cache_size = nodes / 64;
    aytdd::mgr                  = aytdd_bcdd_manager_new(&config);
    if (aytdd::mgr == nullptr) { throw std::runtime_error("aytdd: could not create a manager"); }
  }

  ~aytdd_bcdd_adapter()
  {
    aytdd_bcdd_subst_free(_subst_relnext);
    aytdd_bcdd_subst_free(_subst_relprev);
    aytdd_bcdd_manager_free(aytdd::mgr);
    aytdd::mgr = nullptr;
  }

  template <typename F>
  int
  run(const F& f)
  {
    return f();
  }

  // BDD Operations
public:
  inline aytdd::bcdd_function
  top()
  {
    return aytdd_bcdd_top();
  }

  inline aytdd::bcdd_function
  bot()
  {
    return aytdd_bcdd_bot();
  }

  inline aytdd::bcdd_function
  ithvar(uint32_t label)
  {
    return aytdd_bcdd_var(aytdd::mgr, label);
  }

  inline aytdd::bcdd_function
  nithvar(uint32_t label)
  {
    return aytdd_bcdd_lit(aytdd::mgr, label, false);
  }

  /// \brief The conjunction of the variables of a range, in any order and with repetitions.
  template <typename IT>
  inline aytdd::bcdd_function
  cube(IT rbegin, IT rend)
  {
    const std::vector<uint32_t> vars(rbegin, rend);
    return aytdd_bcdd_cube(aytdd::mgr, vars.data(), vars.size());
  }

  /// \brief The conjunction of the variables that satisfy a predicate.
  inline aytdd::bcdd_function
  cube(const std::function<bool(int)>& pred)
  {
    std::vector<uint32_t> vars;
    for (int i = 0; i < _varcount; ++i) {
      if (pred(i)) { vars.push_back(i); }
    }
    return aytdd_bcdd_cube(aytdd::mgr, vars.data(), vars.size());
  }

  inline aytdd::bcdd_function
  apply_and(const aytdd::bcdd_function& f, const aytdd::bcdd_function& g)
  {
    return f & g;
  }

  inline aytdd::bcdd_function
  apply_or(const aytdd::bcdd_function& f, const aytdd::bcdd_function& g)
  {
    return f | g;
  }

  inline aytdd::bcdd_function
  apply_diff(const aytdd::bcdd_function& f, const aytdd::bcdd_function& g)
  {
    return f - g;
  }

  inline aytdd::bcdd_function
  apply_imp(const aytdd::bcdd_function& f, const aytdd::bcdd_function& g)
  {
    return aytdd_bcdd_imp(aytdd::mgr, f.f, g.f);
  }

  inline aytdd::bcdd_function
  apply_xor(const aytdd::bcdd_function& f, const aytdd::bcdd_function& g)
  {
    return f ^ g;
  }

  inline aytdd::bcdd_function
  apply_xnor(const aytdd::bcdd_function& f, const aytdd::bcdd_function& g)
  {
    return aytdd_bcdd_iff(aytdd::mgr, f.f, g.f);
  }

  inline aytdd::bcdd_function
  ite(const aytdd::bcdd_function& i, const aytdd::bcdd_function& t, const aytdd::bcdd_function& e)
  {
    return aytdd_bcdd_ite(aytdd::mgr, i.f, t.f, e.f);
  }

  template <typename IT>
  inline aytdd::bcdd_function
  extend(const aytdd::bcdd_function& f, IT /*begin*/, IT /*end*/)
  {
    return f;
  }

  inline aytdd::bcdd_function
  exists(const aytdd::bcdd_function& f, int i)
  {
    return aytdd_bcdd_exists(aytdd::mgr, f.f, ithvar(i).f);
  }

  inline aytdd::bcdd_function
  exists(const aytdd::bcdd_function& f, const std::function<bool(int)>& pred)
  {
    return aytdd_bcdd_exists(aytdd::mgr, f.f, cube(pred).f);
  }

  template <typename IT>
  inline aytdd::bcdd_function
  exists(const aytdd::bcdd_function& f, IT rbegin, IT rend)
  {
    return aytdd_bcdd_exists(aytdd::mgr, f.f, cube(rbegin, rend).f);
  }

  inline aytdd::bcdd_function
  forall(const aytdd::bcdd_function& f, int i)
  {
    return aytdd_bcdd_forall(aytdd::mgr, f.f, ithvar(i).f);
  }

  inline aytdd::bcdd_function
  forall(const aytdd::bcdd_function& f, const std::function<bool(int)>& pred)
  {
    return aytdd_bcdd_forall(aytdd::mgr, f.f, cube(pred).f);
  }

  template <typename IT>
  inline aytdd::bcdd_function
  forall(const aytdd::bcdd_function& f, IT rbegin, IT rend)
  {
    return aytdd_bcdd_forall(aytdd::mgr, f.f, cube(rbegin, rend).f);
  }

  /// \brief The successors of 'states' under 'rel', whose current variables are the even ones
  ///        and whose next variables are the odd ones.
  ///
  /// The relational product quantifies the current variables, and the substitution then renames
  /// each next variable to its current one.  The cube and the substitution are built on the first
  /// call and kept, as the other adapters keep their cube and permutation.
  inline aytdd::bcdd_function
  relnext(const aytdd::bcdd_function& states,
          const aytdd::bcdd_function& rel,
          const aytdd::bcdd_function& /*rel_support*/)
  {
    if (_subst_relnext == nullptr) {
      _vars_relnext  = cube([](int x) { return x % 2 == 0; });
      _subst_relnext = aytdd_bcdd_subst_new();
      for (int x = 1; x < _varcount; x += 2) {
        aytdd_bcdd_subst_set(_subst_relnext, x, ithvar(x & ~1).f);
      }
    }
    const aytdd_bcdd next = aytdd_bcdd_and_exists(aytdd::mgr, states.f, rel.f, _vars_relnext.f);
    return aytdd_bcdd_vector_compose(aytdd::mgr, next, _subst_relnext);
  }

  /// \brief The predecessors of 'states' under 'rel', renaming each current variable to its next
  ///        one before the relational product quantifies the next variables.
  inline aytdd::bcdd_function
  relprev(const aytdd::bcdd_function& states,
          const aytdd::bcdd_function& rel,
          const aytdd::bcdd_function& /*rel_support*/)
  {
    if (_subst_relprev == nullptr) {
      _vars_relprev  = cube([](int x) { return x % 2 == 1; });
      _subst_relprev = aytdd_bcdd_subst_new();
      for (int x = 0; x < _varcount; x += 2) {
        aytdd_bcdd_subst_set(_subst_relprev, x, ithvar(x | 1).f);
      }
    }
    const aytdd_bcdd prev = aytdd_bcdd_vector_compose(aytdd::mgr, states.f, _subst_relprev);
    return aytdd_bcdd_and_exists(aytdd::mgr, prev, rel.f, _vars_relprev.f);
  }

  /// \brief The number of nodes reachable from f, the one sink included, as CUDD counts them.
  inline uint64_t
  nodecount(const aytdd::bcdd_function& f)
  {
    return aytdd_bcdd_node_count(aytdd::mgr, f.f);
  }

  inline uint64_t
  satcount(const aytdd::bcdd_function& f)
  {
    return this->satcount(f, this->_varcount);
  }

  /// \brief The number of models of f over 'vc' of the variables, on which f depends only.
  ///
  /// The drivers interleave the counted variables with the others ('relprod', 'game-of-life',
  /// 'hamiltonian'), so the count is over all variables and divided by two for each variable not
  /// counted, as in the BuDDy adapter.
  inline uint64_t
  satcount(const aytdd::bcdd_function& f, const size_t vc)
  {
    assert(vc <= static_cast<size_t>(this->_varcount));

    const double excess_variables = static_cast<double>(this->_varcount) - static_cast<double>(vc);

    return checked_satcount(aytdd_bcdd_sat_count_double(aytdd::mgr, f.f, _varcount)
                            / std::pow(2, excess_variables));
  }

  /// \brief The weighted model count of f over all variables, exactly, stored into 'out'.
  ///
  /// The weights 'weights[x] = (low, high)' of the negative and the positive literal of each
  /// variable 'x', indexed as 'ithvar', form a labeling of literals over the rationals, under sum
  /// and product.
  inline void
  wmc_rat(const aytdd::bcdd_function& f,
          const std::vector<std::pair<mpq_srcptr, mpq_srcptr>>& weights,
          mpq_t out)
  {
    if (weights.size() != static_cast<size_t>(_varcount)) {
      throw std::invalid_argument("aytdd: expected one pair of weights per variable");
    }
    aytdd_labeling* labeling = aytdd_labeling_new(
      _varcount, AYTDD_SEMIRING_SUM_PRODUCT, AYTDD_LABELING_LITERAL, AYTDD_NUMBER_TYPE_RAT);
    for (uint32_t x = 0; x < weights.size(); ++x) {
      aytdd_labeling_set_rat(labeling, x, weights[x].first, weights[x].second);
    }
    aytdd_bcdd_amc_rat(aytdd::mgr, f.f, labeling, out);
    aytdd_labeling_free(labeling);
  }

  /// \brief The weighted model count of f over all variables, in double precision.
  ///
  /// The weights are those of 'wmc_rat', each rounded to the nearest double.
  inline double
  wmc(const aytdd::bcdd_function& f, const std::vector<std::pair<double, double>>& weights)
  {
    if (weights.size() != static_cast<size_t>(_varcount)) {
      throw std::invalid_argument("aytdd: expected one pair of weights per variable");
    }
    aytdd_labeling* labeling = aytdd_labeling_new(
      _varcount, AYTDD_SEMIRING_SUM_PRODUCT, AYTDD_LABELING_LITERAL, AYTDD_NUMBER_TYPE_DOUBLE);
    for (uint32_t x = 0; x < weights.size(); ++x) {
      aytdd_labeling_set_double(labeling, x, weights[x].first, weights[x].second);
    }
    const double count = aytdd_bcdd_amc_double(aytdd::mgr, f.f, labeling);
    aytdd_labeling_free(labeling);
    return count;
  }

  /// \brief The cube of the literals of a path from f to true, false when f is false.
  ///
  /// The path takes the low child of each decision unless it is false, as BuDDy's 'bdd_satone'
  /// does.
  inline aytdd::bcdd_function
  satone(const aytdd::bcdd_function& f)
  {
    return aytdd_bcdd_sat_one(aytdd::mgr, f.f);
  }

  inline aytdd::bcdd_function
  satone(const aytdd::bcdd_function&, const aytdd::bcdd_function&)
  {
    throw std::logic_error("aytdd: a satisfying cube over a set of variables is not supported");
  }

  /// \brief The literals of the path of 'satone', by increasing variable, none at a constant.
  inline std::vector<std::pair<uint32_t, char>>
  pickcube(const aytdd::bcdd_function& f)
  {
    std::vector<aytdd_literal> lits(_varcount);
    const size_t n = aytdd_bcdd_pick_cube(aytdd::mgr, f.f, lits.data(), lits.size());
    // A path has at most one literal per variable.
    assert(n <= lits.size());

    std::vector<std::pair<uint32_t, char>> res;
    res.reserve(n);
    for (size_t i = 0; i < n; ++i) {
      res.push_back({ lits[i].var, static_cast<char>('0' + lits[i].positive) });
    }
    return res;
  }

  void
  print_dot(const aytdd::bcdd_function&, const std::string&)
  {
    std::cerr << "aytdd_bcdd_adapter does not support dot export" << std::endl;
  }

  void
  save(const aytdd::bcdd_function&, const std::string&)
  {
    std::cerr << "aytdd_bcdd_adapter does not support BCDD export" << std::endl;
  }

  // BDD Build Operations
public:
  inline aytdd::bcdd_function
  build_node(const bool value)
  {
    const aytdd::bcdd_function res = value ? top() : bot();
    if (_latest_build == top() || _latest_build == bot()) { _latest_build = res; }
    return res;
  }

  /// \brief The decision on 'label' with the given children, interned and reduced.
  ///
  /// Both children are over variables greater than 'label', since the drivers build a diagram
  /// from its sinks upwards, which is the precondition of the raw constructor, so this needs no
  /// if-then-else.  Equal children are allowed, as the constructor then returns the child.
  inline aytdd::bcdd_function
  build_node(const uint32_t label,
             const aytdd::bcdd_function& low,
             const aytdd::bcdd_function& high)
  {
    return _latest_build = aytdd_bcdd_node(aytdd::mgr, label, low.f, high.f);
  }

  inline aytdd::bcdd_function
  build()
  {
    const aytdd::bcdd_function res = _latest_build;
    _latest_build                  = bot();
    return res;
  }

  // Statistics
public:
  /// \brief The number of decisions the manager holds, reachable or not, since no node is freed.
  inline size_t
  allocated_nodes()
  {
    return aytdd_bcdd_allocated_nodes(aytdd::mgr);
  }

  void
  print_stats()
  {}
};
