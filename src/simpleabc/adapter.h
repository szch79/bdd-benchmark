#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../common/adapter.h"

#include <misc/extra/extra.h>

namespace simpleabc
{
  /// \brief The one manager of the process.
  ///
  /// A literal of the package is a bare word that is meaningful only with the manager that
  /// produced it, and each benchmark builds exactly one manager, so the adapter keeps it here
  /// rather than inside every BDD.
  inline Abc_BddMan* mgr = nullptr;

  /// \brief The memo tables of the two vector compositions of the adapter, one for each of its two
  ///        substitutions.
  ///
  /// The package memoises a composition in a table the caller provides and keeps across calls with
  /// the same substitution, indexed by node, with the result plus one as entry and zero for an
  /// empty one.  A garbage collection frees nodes whose slots are then reused, so the entries are
  /// cleared after each collection.  A table has a word per slot of the node table and is
  /// allocated by the first composition that uses it.
  inline unsigned* memo_relnext = nullptr;
  inline unsigned* memo_relprev = nullptr;

  /// \brief A BDD of that manager, registered as a root while it lives.
  ///
  /// The package has no reference counts, and its garbage collection marks from a list of roots
  /// the caller supplies.  Every BDD links itself into one intrusive list on construction and out
  /// of it on destruction, so that the list holds exactly the literals the drivers can still reach
  /// when a collection is needed.
  struct bcdd_function
  {
    unsigned f = Abc_BddLitConst0();

  private:
    bcdd_function* _prev = nullptr;
    bcdd_function* _next = nullptr;

    inline static bcdd_function* _roots = nullptr;

    void
    link() noexcept
    {
      _next = _roots;
      if (_roots) { _roots->_prev = this; }
      _roots = this;
    }

    void
    unlink() noexcept
    {
      if (_prev) {
        _prev->_next = _next;
      } else {
        _roots = _next;
      }
      if (_next) { _next->_prev = _prev; }
    }

  public:
    bcdd_function() noexcept
    {
      link();
    }

    bcdd_function(unsigned f) noexcept
      : f(f)
    {
      link();
    }

    bcdd_function(const bcdd_function& o) noexcept
      : f(o.f)
    {
      link();
    }

    bcdd_function(bcdd_function&& o) noexcept
      : f(o.f)
    {
      link();
    }

    ~bcdd_function() noexcept
    {
      unlink();
    }

    bcdd_function&
    operator=(const bcdd_function& o) noexcept
    {
      f = o.f;
      return *this;
    }

    bcdd_function&
    operator=(bcdd_function&& o) noexcept
    {
      f = o.f;
      return *this;
    }

    /// \brief The literals of all live BDDs, as the garbage collection wants them.
    static Vec_Int_t*
    roots()
    {
      Vec_Int_t* v = Vec_IntAlloc(64);
      for (const bcdd_function* r = _roots; r; r = r->_next) { Vec_IntPush(v, r->f); }
      return v;
    }

    bool
    operator==(const bcdd_function& o) const noexcept
    {
      return f == o.f;
    }

    bool
    operator!=(const bcdd_function& o) const noexcept
    {
      return f != o.f;
    }

    bcdd_function
    operator~() const noexcept
    {
      return Abc_BddLitNot(f);
    }

    bcdd_function
    operator&(const bcdd_function& o) const;

    bcdd_function
    operator|(const bcdd_function& o) const;

    bcdd_function
    operator^(const bcdd_function& o) const;

    bcdd_function
    operator-(const bcdd_function& o) const;

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

  /// \brief Runs an operation of the package, collecting garbage and retrying once if its node
  ///        table is full.
  ///
  /// The package returns the invalid literal when it cannot allocate a node.  The operands of the
  /// operation are live BDDs, so they survive the collection, while the nodes of the failed attempt
  /// are reclaimed.  A second failure means the table is too small for the result.
  template <typename F>
  inline unsigned
  retrying(const F& op)
  {
    unsigned res = op();
    if (!Abc_BddLitIsInvalid(res)) { return res; }

    Vec_Int_t* roots = bcdd_function::roots();
    Abc_BddGarbageCollect(mgr, roots);
    Vec_IntFree(roots);
    for (unsigned* memo : { memo_relnext, memo_relprev }) {
      if (memo) { std::fill_n(memo, mgr->nObjsAlloc, 0u); }
    }

    res = op();
    if (Abc_BddLitIsInvalid(res)) {
      exit_out_of_memory("simpleabc: out of memory, the node table is full");
    }
    return res;
  }

  inline bcdd_function
  bcdd_function::operator&(const bcdd_function& o) const
  {
    return retrying([&]() { return Abc_BddAnd(mgr, f, o.f); });
  }

  inline bcdd_function
  bcdd_function::operator|(const bcdd_function& o) const
  {
    return retrying([&]() { return Abc_BddOr(mgr, f, o.f); });
  }

  inline bcdd_function
  bcdd_function::operator^(const bcdd_function& o) const
  {
    return retrying([&]() { return Abc_BddLitNot(Abc_BddXnor(mgr, f, o.f)); });
  }

  inline bcdd_function
  bcdd_function::operator-(const bcdd_function& o) const
  {
    return retrying([&]() { return Abc_BddAnd(mgr, f, Abc_BddLitNot(o.f)); });
  }
} // namespace simpleabc

class simpleabc_bcdd_adapter
{
public:
  static constexpr std::string_view name = "SimpleABC";
  static constexpr std::string_view dd   = "BCDD";

  static constexpr bool needs_extend     = false;
  static constexpr bool needs_frame_rule = true;

  static constexpr bool complement_edges = true;

  using dd_t         = simpleabc::bcdd_function;
  using build_node_t = simpleabc::bcdd_function;

private:
  const int _varcount;
  simpleabc::bcdd_function _latest_build;

  Vec_Int_t* _vars_relnext  = nullptr;
  Vec_Int_t* _subst_relnext = nullptr;

  Vec_Int_t* _vars_relprev  = nullptr;
  Vec_Int_t* _subst_relprev = nullptr;

  // Init and Deinit
public:
  /// \brief A manager with a fixed node table, the natural variable order and no reordering.
  ///
  /// Each slot of the table takes 30 bytes, or 31 with more than 254 variables, as the sum of a
  /// unique-table head, a chain link, the two children, the variable, the mark and a 12-byte entry
  /// of the direct-mapped cache of conjunctions.  The memo tables of the compositions, a word per
  /// slot each, are allocated only by a composition and are not counted.  The table holds the
  /// largest power of two of slots that fits in '-M'.  The package grows its table only through
  /// its own driver, which is not used here, so the budget is fixed, as for the other packages;
  /// garbage is collected by the adapter when the table fills.
  /// The package needs a slot for each variable and one for the constant, and it aborts on a
  /// smaller table, so the adapter reports a '-M' too small for that.
  simpleabc_bcdd_adapter(int varcount)
    : _varcount(varcount)
  {
    const size_t bytes_per_slot = varcount < 0xff ? 30 : 31;
    const size_t bytes          = static_cast<size_t>(M) * 1024 * 1024;
    const size_t max_slots      = std::min<size_t>(bytes / bytes_per_slot, 1u << 30);

    unsigned slots = 1;
    while (static_cast<size_t>(slots) * 2 <= max_slots) { slots *= 2; }

    if (slots < static_cast<unsigned>(varcount) + 1) {
      std::cerr << "simpleabc: -M gives " << slots << " slots, fewer than the " << varcount
                << " variables plus one\n";
      std::exit(1);
    }

    simpleabc::mgr = Abc_BddManAlloc(varcount, slots, 0, nullptr, 0);
  }

  ~simpleabc_bcdd_adapter()
  {
    std::free(simpleabc::memo_relnext);
    std::free(simpleabc::memo_relprev);
    simpleabc::memo_relnext = nullptr;
    simpleabc::memo_relprev = nullptr;
    if (_vars_relnext) { Vec_IntFree(_vars_relnext); }
    if (_subst_relnext) { Vec_IntFree(_subst_relnext); }
    if (_vars_relprev) { Vec_IntFree(_vars_relprev); }
    if (_subst_relprev) { Vec_IntFree(_subst_relprev); }
    Abc_BddManFree(simpleabc::mgr);
    simpleabc::mgr = nullptr;
  }

  template <typename F>
  int
  run(const F& f)
  {
    return f();
  }

  // BDD Operations
public:
  inline simpleabc::bcdd_function
  top()
  {
    return Abc_BddLitConst1();
  }

  inline simpleabc::bcdd_function
  bot()
  {
    return Abc_BddLitConst0();
  }

  inline simpleabc::bcdd_function
  ithvar(uint32_t label)
  {
    return Abc_BddLitIthVar(label);
  }

  inline simpleabc::bcdd_function
  nithvar(uint32_t label)
  {
    return Abc_BddLitNot(Abc_BddLitIthVar(label));
  }

  /// \brief The conjunction of the variables of a range, in any order and with repetitions.
  ///
  /// The cube is built from the sink upwards by the node constructor, which needs the variables
  /// in decreasing order and without repetitions.
  template <typename IT>
  inline simpleabc::bcdd_function
  cube(IT rbegin, IT rend)
  {
    std::vector<uint32_t> vars(rbegin, rend);
    std::sort(vars.begin(), vars.end(), std::greater<uint32_t>());
    vars.erase(std::unique(vars.begin(), vars.end()), vars.end());

    simpleabc::bcdd_function res = top();
    for (const uint32_t x : vars) { res = make_node(x, bot(), res); }
    return res;
  }

  /// \brief The conjunction of the variables that satisfy a predicate.
  inline simpleabc::bcdd_function
  cube(const std::function<bool(int)>& pred)
  {
    simpleabc::bcdd_function res = top();
    for (int x = _varcount - 1; 0 <= x; --x) {
      if (pred(x)) { res = make_node(x, bot(), res); }
    }
    return res;
  }

  inline simpleabc::bcdd_function
  apply_and(const simpleabc::bcdd_function& f, const simpleabc::bcdd_function& g)
  {
    return f & g;
  }

  inline simpleabc::bcdd_function
  apply_or(const simpleabc::bcdd_function& f, const simpleabc::bcdd_function& g)
  {
    return f | g;
  }

  inline simpleabc::bcdd_function
  apply_diff(const simpleabc::bcdd_function& f, const simpleabc::bcdd_function& g)
  {
    return f - g;
  }

  inline simpleabc::bcdd_function
  apply_imp(const simpleabc::bcdd_function& f, const simpleabc::bcdd_function& g)
  {
    return ~f | g;
  }

  inline simpleabc::bcdd_function
  apply_xor(const simpleabc::bcdd_function& f, const simpleabc::bcdd_function& g)
  {
    return f ^ g;
  }

  inline simpleabc::bcdd_function
  apply_xnor(const simpleabc::bcdd_function& f, const simpleabc::bcdd_function& g)
  {
    return simpleabc::retrying([&]() { return Abc_BddXnor(simpleabc::mgr, f.f, g.f); });
  }

  /// \brief The if-then-else of three BDDs, as two conjunctions and a disjunction.
  ///
  /// The package's recursive if-then-else indexes the cache of conjunctions with a different
  /// stride, and the two cannot share one cache, so the decomposition is used.
  inline simpleabc::bcdd_function
  ite(const simpleabc::bcdd_function& i,
      const simpleabc::bcdd_function& t,
      const simpleabc::bcdd_function& e)
  {
    return simpleabc::retrying([&]() { return Abc_BddIteAnd(simpleabc::mgr, i.f, t.f, e.f); });
  }

  template <typename IT>
  inline simpleabc::bcdd_function
  extend(const simpleabc::bcdd_function& f, IT /*begin*/, IT /*end*/)
  {
    return f;
  }

  inline simpleabc::bcdd_function
  exists(const simpleabc::bcdd_function& f, int i)
  {
    const int var = i;
    return exists(f, &var, &var + 1);
  }

  inline simpleabc::bcdd_function
  exists(const simpleabc::bcdd_function& f, const std::function<bool(int)>& pred)
  {
    const std::vector<int> vars = variables(pred);
    return exists(f, vars.begin(), vars.end());
  }

  /// \brief The existential quantification, as the dual of the universal one of the package.
  template <typename IT>
  inline simpleabc::bcdd_function
  exists(const simpleabc::bcdd_function& f, IT rbegin, IT rend)
  {
    Vec_Int_t* vars              = vec_of(rbegin, rend);
    simpleabc::bcdd_function res = simpleabc::retrying([&]() {
      return Abc_BddLitNot(Abc_BddUnivAbstract(simpleabc::mgr, Abc_BddLitNot(f.f), vars));
    });
    Vec_IntFree(vars);
    return res;
  }

  inline simpleabc::bcdd_function
  forall(const simpleabc::bcdd_function& f, int i)
  {
    const int var = i;
    return forall(f, &var, &var + 1);
  }

  inline simpleabc::bcdd_function
  forall(const simpleabc::bcdd_function& f, const std::function<bool(int)>& pred)
  {
    const std::vector<int> vars = variables(pred);
    return forall(f, vars.begin(), vars.end());
  }

  template <typename IT>
  inline simpleabc::bcdd_function
  forall(const simpleabc::bcdd_function& f, IT rbegin, IT rend)
  {
    Vec_Int_t* vars = vec_of(rbegin, rend);
    simpleabc::bcdd_function res =
      simpleabc::retrying([&]() { return Abc_BddUnivAbstract(simpleabc::mgr, f.f, vars); });
    Vec_IntFree(vars);
    return res;
  }

  /// \brief The successors of 'states' under 'rel', whose current variables are the even ones
  ///        and whose next variables are the odd ones.
  ///
  /// The conjunction is quantified over the current variables, and the vector composition then
  /// renames each next variable to its current one.  The variables and the substitution are built
  /// on the first call and kept, as the other adapters keep their cube and permutation.
  inline simpleabc::bcdd_function
  relnext(const simpleabc::bcdd_function& states,
          const simpleabc::bcdd_function& rel,
          const simpleabc::bcdd_function& /*rel_support*/)
  {
    if (_subst_relnext == nullptr) {
      _vars_relnext  = Vec_IntAlloc(_varcount / 2 + 1);
      _subst_relnext = Vec_IntAlloc(_varcount);
      for (int x = 0; x < _varcount; ++x) {
        if (x % 2 == 0) { Vec_IntPush(_vars_relnext, x); }
        Vec_IntPush(_subst_relnext, Abc_BddLitIthVar(x & ~1));
      }
    }
    const simpleabc::bcdd_function next = exists(states & rel, _vars_relnext);
    return compose(next, _subst_relnext, simpleabc::memo_relnext);
  }

  /// \brief The predecessors of 'states' under 'rel', renaming each current variable to its next
  ///        one before the conjunction is quantified over the next variables.
  inline simpleabc::bcdd_function
  relprev(const simpleabc::bcdd_function& states,
          const simpleabc::bcdd_function& rel,
          const simpleabc::bcdd_function& /*rel_support*/)
  {
    if (_subst_relprev == nullptr) {
      _vars_relprev  = Vec_IntAlloc(_varcount / 2 + 1);
      _subst_relprev = Vec_IntAlloc(_varcount);
      for (int x = 0; x < _varcount; ++x) {
        if (x % 2 == 1) { Vec_IntPush(_vars_relprev, x); }
        Vec_IntPush(_subst_relprev, Abc_BddLitIthVar(x | 1));
      }
    }
    const simpleabc::bcdd_function prev = compose(states, _subst_relprev, simpleabc::memo_relprev);
    return exists(prev & rel, _vars_relprev);
  }

  /// \brief The number of nodes reachable from f, the one sink included, as CUDD counts them.
  inline uint64_t
  nodecount(const simpleabc::bcdd_function& f)
  {
    const uint64_t c = count_marking(f.f);
    Abc_BddUnmark_rec(simpleabc::mgr, f.f);
    return c + 1;
  }

  inline uint64_t
  satcount(const simpleabc::bcdd_function& f)
  {
    return this->satcount(f, this->_varcount);
  }

  /// \brief The number of models of f over 'vc' of the variables, on which f depends only.
  ///
  /// The package has no model count.  As CUDD does, the adapter computes the fraction of all
  /// assignments that satisfy f, which does not depend on the set of variables, and scales it to
  /// the 'vc' variables, so that the count of a bcdd_function over few of many variables does not
  /// overflow on the way.
  inline uint64_t
  satcount(const simpleabc::bcdd_function& f, const size_t vc)
  {
    assert(vc <= static_cast<size_t>(this->_varcount));

    std::unordered_map<unsigned, scaled> memo;
    const scaled p = fraction(f.f, memo);
    return checked_satcount(std::ldexp(p.mantissa, p.exponent + static_cast<int>(vc)));
  }

  /// \brief The cube of the literals of the path of 'pickcube', false when f is false.
  inline simpleabc::bcdd_function
  satone(const simpleabc::bcdd_function& f)
  {
    if (Abc_BddLitIsConst0(f.f)) { return bot(); }

    const std::vector<std::pair<uint32_t, char>> path = pickcube(f);

    simpleabc::bcdd_function res = top();
    for (auto it = path.rbegin(); it != path.rend(); ++it) {
      res = it->second == '1' ? make_node(it->first, bot(), res) : make_node(it->first, res, bot());
    }
    return res;
  }

  inline simpleabc::bcdd_function
  satone(const simpleabc::bcdd_function&, const simpleabc::bcdd_function&)
  {
    throw std::logic_error("simpleabc: a satisfying cube over a set of variables is not supported");
  }

  /// \brief The literals of a path from f to true, by increasing variable, none at a constant.
  ///
  /// The path takes the low child of each decision unless it is false, as BuDDy's 'bdd_satone'
  /// does.
  inline std::vector<std::pair<uint32_t, char>>
  pickcube(const simpleabc::bcdd_function& f)
  {
    std::vector<std::pair<uint32_t, char>> res;
    for (unsigned x = f.f; !Abc_BddLitIsConst(x);) {
      const unsigned lo = Abc_BddElse(simpleabc::mgr, x);
      const bool high   = Abc_BddLitIsConst0(lo);
      res.emplace_back(Abc_BddVar(simpleabc::mgr, x), high ? '1' : '0');
      x = high ? Abc_BddThen(simpleabc::mgr, x) : lo;
    }
    return res;
  }

  void
  print_dot(const simpleabc::bcdd_function&, const std::string&)
  {
    std::cerr << "simpleabc_bcdd_adapter does not support dot export" << std::endl;
  }

  void
  save(const simpleabc::bcdd_function&, const std::string&)
  {
    std::cerr << "simpleabc_bcdd_adapter does not support BCDD export" << std::endl;
  }

  // BDD Build Operations
public:
  inline simpleabc::bcdd_function
  build_node(const bool value)
  {
    const simpleabc::bcdd_function res = value ? top() : bot();
    if (_latest_build == top() || _latest_build == bot()) { _latest_build = res; }
    return res;
  }

  /// \brief The decision on 'label' with the given children, interned and reduced.
  ///
  /// The drivers build a diagram from its sinks upwards, so both children are over variables
  /// greater than 'label', which is the precondition of the node constructor.  Equal children are
  /// allowed, as the constructor then returns the child.
  inline simpleabc::bcdd_function
  build_node(const uint32_t label,
             const simpleabc::bcdd_function& low,
             const simpleabc::bcdd_function& high)
  {
    return _latest_build = make_node(label, low, high);
  }

  inline simpleabc::bcdd_function
  build()
  {
    const simpleabc::bcdd_function res = _latest_build;
    _latest_build                      = bot();
    return res;
  }

  // Statistics
public:
  /// \brief The number of slots of the node table ever used, since freed slots are reused.
  inline size_t
  allocated_nodes()
  {
    return simpleabc::mgr->nObjs;
  }

  void
  print_stats()
  {}

private:
  /// \brief The node on 'label' with the given children, which the variable order of the manager
  ///        being the natural one makes the level too.
  inline simpleabc::bcdd_function
  make_node(const uint32_t label,
            const simpleabc::bcdd_function& low,
            const simpleabc::bcdd_function& high)
  {
    return simpleabc::retrying(
      [&]() { return Abc_BddUniqueCreate(simpleabc::mgr, label, high.f, low.f); });
  }

  /// \brief The number of unmarked nodes reachable from x, which are marked on the way.
  inline uint64_t
  count_marking(const unsigned x)
  {
    if (Abc_BddLitIsConst(x) || Abc_BddMark(simpleabc::mgr, x)) { return 0; }
    Abc_BddSetMark(simpleabc::mgr, x, 1);
    return 1 + count_marking(Abc_BddElse(simpleabc::mgr, x))
      + count_marking(Abc_BddThen(simpleabc::mgr, x));
  }

  inline std::vector<int>
  variables(const std::function<bool(int)>& pred)
  {
    std::vector<int> vars;
    for (int x = 0; x < _varcount; ++x) {
      if (pred(x)) { vars.push_back(x); }
    }
    return vars;
  }

  template <typename IT>
  inline Vec_Int_t*
  vec_of(IT begin, IT end)
  {
    Vec_Int_t* v = Vec_IntAlloc(16);
    for (; begin != end; ++begin) { Vec_IntPush(v, *begin); }
    return v;
  }

  inline simpleabc::bcdd_function
  exists(const simpleabc::bcdd_function& f, Vec_Int_t* vars)
  {
    return simpleabc::retrying([&]() {
      return Abc_BddLitNot(Abc_BddUnivAbstract(simpleabc::mgr, Abc_BddLitNot(f.f), vars));
    });
  }

  /// \brief The simultaneous substitution of a literal for each variable of f, memoised in the
  ///        memo table of the substitution, which is allocated on the first call.
  inline simpleabc::bcdd_function
  compose(const simpleabc::bcdd_function& f, Vec_Int_t* subst, unsigned*& memo)
  {
    if (!memo) {
      memo = static_cast<unsigned*>(std::calloc(simpleabc::mgr->nObjsAlloc, sizeof(unsigned)));
      if (!memo) { throw std::bad_alloc(); }
    }
    return simpleabc::retrying(
      [&]() { return Abc_BddVectorCompose(simpleabc::mgr, f.f, subst, memo, 1); });
  }

  /// \brief A fraction 'mantissa * 2^exponent' with the mantissa in [2^-64, 1], or zero.
  ///
  /// With thousands of variables the fraction of a subfunction falls below the range of a
  /// double, so the exponent is kept apart.
  struct scaled
  {
    double mantissa;
    int exponent;

    static scaled
    normalized(double mantissa, int exponent)
    {
      while (mantissa != 0.0 && mantissa < std::ldexp(1.0, -64)) {
        mantissa = std::ldexp(mantissa, 64);
        exponent -= 64;
      }
      return { mantissa, exponent };
    }

    scaled
    mean(const scaled o) const
    {
      if (mantissa == 0.0) { return normalized(o.mantissa, o.exponent - 1); }
      if (o.mantissa == 0.0) { return normalized(mantissa, exponent - 1); }
      const int e = std::max(exponent, o.exponent);
      return normalized(std::ldexp(mantissa, exponent - e) + std::ldexp(o.mantissa, o.exponent - e),
                        e - 1);
    }

    scaled
    complement() const
    {
      return normalized(1.0 - std::ldexp(mantissa, exponent), 0);
    }
  };

  /// \brief The fraction of the assignments that satisfy the bcdd_function of a literal, memoised
  /// on
  ///        regular literals and complemented by subtraction from one.
  ///
  /// A node's fraction is the mean of its children's, since each child is the bcdd_function under
  /// one value of the variable, and the variables between a node and its child do not change it.
  inline scaled
  fraction(const unsigned x, std::unordered_map<unsigned, scaled>& memo)
  {
    if (Abc_BddLitIsConst(x)) { return { Abc_BddLitIsConst1(x) ? 1.0 : 0.0, 0 }; }

    const unsigned reg = Abc_BddLitRegular(x);

    scaled p;
    const auto it = memo.find(reg);
    if (it != memo.end()) {
      p = it->second;
    } else {
      p = fraction(Abc_BddThen(simpleabc::mgr, reg), memo)
            .mean(fraction(Abc_BddElse(simpleabc::mgr, reg), memo));
      memo.emplace(reg, p);
    }
    return Abc_BddLitIsCompl(x) ? p.complement() : p;
  }
};
