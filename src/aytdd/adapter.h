#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "../common/adapter.h"

#include <aytdd/bcdd.h>

namespace aytdd
{
  /// \brief The one manager of the process.
  ///
  /// Every edge of the C interface is a bare word that is meaningful only with the manager
  /// that produced it, and each benchmark builds exactly one manager, so the adapter keeps it
  /// here rather than inside every function object.
  inline aytdd_bcdd_manager* mgr = nullptr;

  /// \brief A BDD of that manager, with the operators the drivers use.
  ///
  /// The package does not reclaim nodes, so a function object owns nothing and copying one is
  /// copying a word.
  struct function
  {
    aytdd_bcdd f = aytdd_bcdd_false();

    function() = default;

    function(aytdd_bcdd f)
      : f(f)
    {}

    bool
    operator==(const function& o) const
    {
      return f == o.f;
    }

    bool
    operator!=(const function& o) const
    {
      return f != o.f;
    }

    function
    operator~() const
    {
      return aytdd_bcdd_not(f);
    }

    function
    operator&(const function& o) const
    {
      return aytdd_bcdd_and(mgr, f, o.f);
    }

    function
    operator|(const function& o) const
    {
      return aytdd_bcdd_or(mgr, f, o.f);
    }

    function
    operator^(const function& o) const
    {
      return aytdd_bcdd_xor(mgr, f, o.f);
    }

    function
    operator-(const function& o) const
    {
      return aytdd_bcdd_diff(mgr, f, o.f);
    }

    function&
    operator&=(const function& o)
    {
      return *this = *this & o;
    }

    function&
    operator|=(const function& o)
    {
      return *this = *this | o;
    }

    function&
    operator^=(const function& o)
    {
      return *this = *this ^ o;
    }

    function&
    operator-=(const function& o)
    {
      return *this = *this - o;
    }
  };
} // namespace aytdd

class aytdd_bcdd_adapter
{
public:
  static constexpr std::string_view name = "Aytdd";
  static constexpr std::string_view dd   = "BCDD";

  static constexpr bool needs_extend     = false;
  static constexpr bool needs_frame_rule = true;

  static constexpr bool complement_edges = true;

  using dd_t         = aytdd::function;
  using build_node_t = aytdd::function;

private:
  const int _varcount;
  aytdd::function _latest_build;

  // Init and Deinit
public:
  aytdd_bcdd_adapter(int varcount)
    : _varcount(varcount)
  {
    // The package never reclaims nodes.  Its node table grows beyond the reservation made here
    // for half of '-M' at roughly 24 bytes per decision, while each computed table keeps a fixed
    // number of 16-byte slots.  The conjunction table takes a sixth of the other half, and the
    // exclusive-disjunction and if-then-else tables an eighth of the conjunction table each.
    const size_t bytes = static_cast<size_t>(M) * 1024 * 1024;
    aytdd_bcdd_config config{};
    config.node_table_size = bytes / 2 / 24;
    config.and_cache_size  = bytes / 2 / 96;
    config.xor_cache_size  = config.and_cache_size / 8;
    config.ite_cache_size  = config.and_cache_size / 8;
    aytdd::mgr             = aytdd_bcdd_manager_new(&config);
    if (aytdd::mgr == nullptr) { throw std::runtime_error("aytdd: could not create a manager"); }
    if (enable_reordering) {
      std::cerr << "aytdd: variable reordering is not supported, ignoring -R" << std::endl;
    }
  }

  ~aytdd_bcdd_adapter()
  {
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
  inline aytdd::function
  top()
  {
    return aytdd_bcdd_true();
  }

  inline aytdd::function
  bot()
  {
    return aytdd_bcdd_false();
  }

  inline aytdd::function
  ithvar(uint32_t label)
  {
    return aytdd_bcdd_var(aytdd::mgr, label);
  }

  inline aytdd::function
  nithvar(uint32_t label)
  {
    return aytdd_bcdd_lit(aytdd::mgr, label, false);
  }

  template <typename IT>
  inline aytdd::function
  cube(IT rbegin, IT rend)
  {
    aytdd::function res = top();
    while (rbegin != rend) { res = ite(ithvar(*(rbegin++)), std::move(res), bot()); }
    return res;
  }

  inline aytdd::function
  cube(const std::function<bool(int)>& pred)
  {
    aytdd::function res = top();
    for (int i = _varcount - 1; 0 <= i; --i) {
      if (pred(i)) { res = ite(ithvar(i), std::move(res), bot()); }
    }
    return res;
  }

  inline aytdd::function
  apply_and(const aytdd::function& f, const aytdd::function& g)
  {
    return f & g;
  }

  inline aytdd::function
  apply_or(const aytdd::function& f, const aytdd::function& g)
  {
    return f | g;
  }

  inline aytdd::function
  apply_diff(const aytdd::function& f, const aytdd::function& g)
  {
    return f - g;
  }

  inline aytdd::function
  apply_imp(const aytdd::function& f, const aytdd::function& g)
  {
    return aytdd_bcdd_imp(aytdd::mgr, f.f, g.f);
  }

  inline aytdd::function
  apply_xor(const aytdd::function& f, const aytdd::function& g)
  {
    return f ^ g;
  }

  inline aytdd::function
  apply_xnor(const aytdd::function& f, const aytdd::function& g)
  {
    return aytdd_bcdd_iff(aytdd::mgr, f.f, g.f);
  }

  inline aytdd::function
  ite(const aytdd::function& i, const aytdd::function& t, const aytdd::function& e)
  {
    return aytdd_bcdd_ite(aytdd::mgr, i.f, t.f, e.f);
  }

  template <typename IT>
  inline aytdd::function
  extend(const aytdd::function& f, IT /*begin*/, IT /*end*/)
  {
    return f;
  }

  inline aytdd::function
  exists(const aytdd::function&, int)
  {
    throw std::logic_error("aytdd: quantification is not supported");
  }

  inline aytdd::function
  exists(const aytdd::function&, const std::function<bool(int)>&)
  {
    throw std::logic_error("aytdd: quantification is not supported");
  }

  template <typename IT>
  inline aytdd::function
  exists(const aytdd::function&, IT, IT)
  {
    throw std::logic_error("aytdd: quantification is not supported");
  }

  inline aytdd::function
  forall(const aytdd::function&, int)
  {
    throw std::logic_error("aytdd: quantification is not supported");
  }

  inline aytdd::function
  forall(const aytdd::function&, const std::function<bool(int)>&)
  {
    throw std::logic_error("aytdd: quantification is not supported");
  }

  template <typename IT>
  inline aytdd::function
  forall(const aytdd::function&, IT, IT)
  {
    throw std::logic_error("aytdd: quantification is not supported");
  }

  inline aytdd::function
  relnext(const aytdd::function&, const aytdd::function&, const aytdd::function&)
  {
    throw std::logic_error("aytdd: relational products are not supported");
  }

  inline aytdd::function
  relprev(const aytdd::function&, const aytdd::function&, const aytdd::function&)
  {
    throw std::logic_error("aytdd: relational products are not supported");
  }

  /// \brief The number of nodes reachable from f, the one sink included, as CUDD counts them.
  inline uint64_t
  nodecount(const aytdd::function& f)
  {
    return aytdd_bcdd_node_count(aytdd::mgr, f.f);
  }

  inline uint64_t
  satcount(const aytdd::function& f)
  {
    return static_cast<uint64_t>(aytdd_bcdd_sat_count(aytdd::mgr, f.f, _varcount));
  }

  inline uint64_t
  satcount(const aytdd::function& f, const size_t vc)
  {
    assert(vc <= static_cast<size_t>(this->_varcount));

    const double excess_variables = static_cast<double>(this->_varcount) - static_cast<double>(vc);

    return static_cast<uint64_t>(aytdd_bcdd_sat_count(aytdd::mgr, f.f, _varcount)
                                 / std::pow(2, excess_variables));
  }

  inline aytdd::function
  satone(const aytdd::function&)
  {
    throw std::logic_error("aytdd: witness extraction is not supported");
  }

  inline aytdd::function
  satone(const aytdd::function&, const aytdd::function&)
  {
    throw std::logic_error("aytdd: witness extraction is not supported");
  }

  inline std::vector<std::pair<uint32_t, char>>
  pickcube(const aytdd::function&)
  {
    throw std::logic_error("aytdd: witness extraction is not supported");
  }

  void
  print_dot(const aytdd::function&, const std::string&)
  {
    std::cerr << "aytdd_bcdd_adapter does not support dot export" << std::endl;
  }

  void
  save(const aytdd::function&, const std::string&)
  {
    throw std::logic_error("aytdd: serialisation is not supported");
  }

  aytdd::function
  load(const std::string&)
  {
    throw std::logic_error("aytdd: serialisation is not supported");
  }

  // BDD Build Operations
public:
  inline aytdd::function
  build_node(const bool value)
  {
    const aytdd::function res = value ? top() : bot();
    if (_latest_build == top() || _latest_build == bot()) { _latest_build = res; }
    return res;
  }

  /// \brief The decision on 'label' with the given children, interned and reduced.
  ///
  /// The drivers feed decisions bottom up over increasing levels, which is exactly the
  /// precondition of the raw constructor, so this needs no if-then-else.
  inline aytdd::function
  build_node(const uint32_t label, const aytdd::function& low, const aytdd::function& high)
  {
    return _latest_build = aytdd_bcdd_node(aytdd::mgr, label, low.f, high.f);
  }

  inline aytdd::function
  build()
  {
    const aytdd::function res = _latest_build;
    _latest_build             = bot();
    return res;
  }

  // Statistics
public:
  /// \brief Not reported, as for Sylvan.
  ///
  /// The C interface exposes no count of the decisions the manager holds, and only picotrav
  /// records this statistic.
  inline size_t
  allocated_nodes()
  {
    return 0;
  }

  void
  print_stats()
  {}
};
