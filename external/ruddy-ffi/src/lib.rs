#![allow(non_camel_case_types)]
#![allow(clippy::missing_safety_doc)]
#![deny(unsafe_op_in_unsafe_fn)]

//! A C interface for the shared representation of ruddy.
//!
//! A manager is a boxed `BddManager`.  A BDD is a boxed `Bdd`, the reference-counted handle that
//! keeps its nodes alive across garbage collections, together with a pointer to its manager, so
//! that the operations on BDDs take no manager.  Both are wrapped in local types, which the
//! generated header declares as opaque structures.  Every function that returns a BDD returns a
//! new box, which the caller releases with `ruddy_bdd_free`.  Releasing a BDD does not touch its
//! manager.  Handles hold an `Rc`, so the interface is single-threaded.

use std::collections::HashMap;

use ruddy::boolean_operators::BooleanOperator;
use ruddy::shared::{Bdd, BddManager};
use ruddy::split::Bdd as SplitBdd;
use ruddy::{NodeId, NodeIdAny, VariableId};

pub struct Manager(BddManager);

pub struct Function {
    bdd: Bdd,
    manager: *mut Manager,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ruddy_manager_t {
    _p: *mut Manager,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ruddy_bdd_t {
    _p: *mut Function,
}

impl ruddy_manager_t {
    unsafe fn get<'a>(self) -> &'a mut BddManager {
        debug_assert!(!self._p.is_null());
        unsafe { &mut (*self._p).0 }
    }
}

impl ruddy_bdd_t {
    fn new(bdd: Bdd, manager: *mut Manager) -> Self {
        Self {
            _p: Box::into_raw(Box::new(Function { bdd, manager })),
        }
    }

    unsafe fn get<'a>(self) -> &'a Bdd {
        debug_assert!(!self._p.is_null());
        unsafe { &(*self._p).bdd }
    }

    unsafe fn manager<'a>(self) -> &'a mut BddManager {
        debug_assert!(!self._p.is_null());
        unsafe { &mut (*(*self._p).manager).0 }
    }

    /// A new BDD of the manager of `self`, computed from that manager and `self`.
    unsafe fn map(self, op: impl FnOnce(&mut BddManager, &Bdd) -> Bdd) -> ruddy_bdd_t {
        let manager = unsafe { (*self._p).manager };
        ruddy_bdd_t::new(op(unsafe { self.manager() }, unsafe { self.get() }), manager)
    }
}

/// The difference `left & !right`, through ruddy's generic apply.
///
/// The library provides conjunction, disjunction, exclusive disjunction, implication and
/// equivalence as operators of its apply.  Negation rebuilds the whole diagram, since there are no
/// complement edges, so the difference is defined here as one more operator rather than as a
/// conjunction with a negation.
#[derive(Clone, Copy)]
struct AndNot;

impl BooleanOperator for AndNot {
    fn for_split<TId1: NodeIdAny, TId2: NodeIdAny, TResultId: NodeIdAny>(
        self,
    ) -> impl Fn(TId1, TId2) -> TResultId {
        |left, right| {
            if left.is_zero() || right.is_one() {
                TResultId::zero()
            } else if left.is_one() && right.is_zero() {
                TResultId::one()
            } else {
                TResultId::undefined()
            }
        }
    }

    fn for_shared<TId: NodeIdAny>(self) -> impl Fn(TId, TId) -> TId {
        |left, right| {
            if left.is_zero() || right.is_one() || left == right {
                TId::zero()
            } else if right.is_zero() {
                left
            } else {
                TId::undefined()
            }
        }
    }
}

// Manager

#[no_mangle]
pub extern "C" fn ruddy_manager_new() -> ruddy_manager_t {
    ruddy_manager_t {
        _p: Box::into_raw(Box::new(Manager(BddManager::new()))),
    }
}

#[no_mangle]
pub unsafe extern "C" fn ruddy_manager_free(manager: ruddy_manager_t) {
    if !manager._p.is_null() {
        drop(unsafe { Box::from_raw(manager._p) });
    }
}

/// The number of nodes in the manager's table, live or not yet collected.
#[no_mangle]
pub unsafe extern "C" fn ruddy_manager_node_count(manager: ruddy_manager_t) -> usize {
    unsafe { manager.get() }.total_node_count()
}

// Construction

#[no_mangle]
pub unsafe extern "C" fn ruddy_true(manager: ruddy_manager_t) -> ruddy_bdd_t {
    ruddy_bdd_t::new(unsafe { manager.get() }.new_bdd_true(), manager._p)
}

#[no_mangle]
pub unsafe extern "C" fn ruddy_false(manager: ruddy_manager_t) -> ruddy_bdd_t {
    ruddy_bdd_t::new(unsafe { manager.get() }.new_bdd_false(), manager._p)
}

#[no_mangle]
pub unsafe extern "C" fn ruddy_literal(
    manager: ruddy_manager_t,
    var: u32,
    value: bool,
) -> ruddy_bdd_t {
    let m = unsafe { manager.get() };
    ruddy_bdd_t::new(m.new_bdd_literal(VariableId::new(var), value), manager._p)
}

/// The node on `var` with children `then` and `else_`, both of which must be over variables
/// greater than `var`.
#[no_mangle]
pub unsafe extern "C" fn ruddy_if_then_else(
    manager: ruddy_manager_t,
    var: u32,
    then: ruddy_bdd_t,
    else_: ruddy_bdd_t,
) -> ruddy_bdd_t {
    let m = unsafe { manager.get() };
    let (then, else_) = unsafe { (then.get(), else_.get()) };
    ruddy_bdd_t::new(m.if_then_else(VariableId::new(var), then, else_), manager._p)
}

// Handles

#[no_mangle]
pub unsafe extern "C" fn ruddy_bdd_clone(f: ruddy_bdd_t) -> ruddy_bdd_t {
    ruddy_bdd_t::new(unsafe { f.get() }.clone(), unsafe { (*f._p).manager })
}

#[no_mangle]
pub unsafe extern "C" fn ruddy_bdd_free(f: ruddy_bdd_t) {
    if !f._p.is_null() {
        drop(unsafe { Box::from_raw(f._p) });
    }
}

#[no_mangle]
pub unsafe extern "C" fn ruddy_bdd_eq(f: ruddy_bdd_t, g: ruddy_bdd_t) -> bool {
    unsafe { f.get() == g.get() }
}

// Operations

#[no_mangle]
pub unsafe extern "C" fn ruddy_not(f: ruddy_bdd_t) -> ruddy_bdd_t {
    unsafe { f.map(|m, f| m.not(f)) }
}

#[no_mangle]
pub unsafe extern "C" fn ruddy_and(f: ruddy_bdd_t, g: ruddy_bdd_t) -> ruddy_bdd_t {
    let g = unsafe { g.get() };
    unsafe { f.map(|m, f| m.and(f, g)) }
}

#[no_mangle]
pub unsafe extern "C" fn ruddy_or(f: ruddy_bdd_t, g: ruddy_bdd_t) -> ruddy_bdd_t {
    let g = unsafe { g.get() };
    unsafe { f.map(|m, f| m.or(f, g)) }
}

#[no_mangle]
pub unsafe extern "C" fn ruddy_xor(f: ruddy_bdd_t, g: ruddy_bdd_t) -> ruddy_bdd_t {
    let g = unsafe { g.get() };
    unsafe { f.map(|m, f| m.xor(f, g)) }
}

#[no_mangle]
pub unsafe extern "C" fn ruddy_imp(f: ruddy_bdd_t, g: ruddy_bdd_t) -> ruddy_bdd_t {
    let g = unsafe { g.get() };
    unsafe { f.map(|m, f| m.implies(f, g)) }
}

#[no_mangle]
pub unsafe extern "C" fn ruddy_iff(f: ruddy_bdd_t, g: ruddy_bdd_t) -> ruddy_bdd_t {
    let g = unsafe { g.get() };
    unsafe { f.map(|m, f| m.iff(f, g)) }
}

#[no_mangle]
pub unsafe extern "C" fn ruddy_and_not(f: ruddy_bdd_t, g: ruddy_bdd_t) -> ruddy_bdd_t {
    let g = unsafe { g.get() };
    unsafe { f.map(|m, f| m.apply(f, g, AndNot)) }
}

unsafe fn variables<'a>(vars: *const u32, num_vars: usize) -> Vec<VariableId> {
    if num_vars == 0 {
        return Vec::new();
    }
    let vars = unsafe { std::slice::from_raw_parts(vars, num_vars) };
    vars.iter().map(|&v| VariableId::new(v)).collect()
}

#[no_mangle]
pub unsafe extern "C" fn ruddy_exists(
    f: ruddy_bdd_t,
    vars: *const u32,
    num_vars: usize,
) -> ruddy_bdd_t {
    let vars = unsafe { variables(vars, num_vars) };
    unsafe { f.map(|m, f| m.exists(f, &vars)) }
}

#[no_mangle]
pub unsafe extern "C" fn ruddy_forall(
    f: ruddy_bdd_t,
    vars: *const u32,
    num_vars: usize,
) -> ruddy_bdd_t {
    let vars = unsafe { variables(vars, num_vars) };
    unsafe { f.map(|m, f| m.for_all(f, &vars)) }
}

// Queries

/// The number of nodes reachable from `f`, counting both terminals when `f` is not constant.
#[no_mangle]
pub unsafe extern "C" fn ruddy_node_count(f: ruddy_bdd_t) -> usize {
    unsafe { f.manager() }.node_count(unsafe { f.get() })
}

/// The number of models of `f` over `num_vars` variables, which need not be the variables `f`
/// depends on.
///
/// The count is the fraction of all assignments that satisfy `f`, which does not depend on the
/// set of variables, scaled to `num_vars` variables at the end, as CUDD counts.  The shared table
/// does not expose its nodes, so the fraction is computed on a split copy of `f`, where a node's
/// fraction is the mean of its children's, since each child is the function under one value of the
/// variable, and the variables between a node and its child do not change it.
#[no_mangle]
pub unsafe extern "C" fn ruddy_sat_count(f: ruddy_bdd_t, num_vars: u32) -> f64 {
    let split = unsafe { f.manager() }.export_split(unsafe { f.get() });
    let terminals = (SplitBdd::new_false().root(), SplitBdd::new_true().root());
    let mut memo: HashMap<NodeId, Scaled> = HashMap::new();
    let p = fraction(&split, split.root(), terminals, &mut memo);
    p.mantissa * 2f64.powi(p.exponent + num_vars as i32)
}

/// A fraction `mantissa * 2^exponent` with the mantissa in `[2^-64, 1]`, or zero.
///
/// With thousands of variables the fraction of a subfunction falls below the range of a double,
/// so the exponent is kept apart.
#[derive(Clone, Copy)]
struct Scaled {
    mantissa: f64,
    exponent: i32,
}

impl Scaled {
    const ZERO: Scaled = Scaled { mantissa: 0.0, exponent: 0 };
    const ONE: Scaled = Scaled { mantissa: 1.0, exponent: 0 };

    fn normalized(mut mantissa: f64, mut exponent: i32) -> Scaled {
        while mantissa != 0.0 && mantissa < 2f64.powi(-64) {
            mantissa *= 2f64.powi(64);
            exponent -= 64;
        }
        Scaled { mantissa, exponent }
    }

    fn mean(self, other: Scaled) -> Scaled {
        if self.mantissa == 0.0 {
            return Scaled::normalized(other.mantissa, other.exponent - 1);
        }
        if other.mantissa == 0.0 {
            return Scaled::normalized(self.mantissa, self.exponent - 1);
        }
        let exponent = self.exponent.max(other.exponent);
        let mantissa = self.mantissa * 2f64.powi(self.exponent - exponent)
            + other.mantissa * 2f64.powi(other.exponent - exponent);
        Scaled::normalized(mantissa, exponent - 1)
    }
}

/// The identifiers of the terminals are not inspectable, so they are passed in as the roots of
/// the constant split BDDs.
fn fraction(
    bdd: &SplitBdd,
    node: NodeId,
    terminals: (NodeId, NodeId),
    memo: &mut HashMap<NodeId, Scaled>,
) -> Scaled {
    if node == terminals.0 {
        return Scaled::ZERO;
    }
    if node == terminals.1 {
        return Scaled::ONE;
    }
    if let Some(&p) = memo.get(&node) {
        return p;
    }
    let (low, high) = bdd.get_links(node);
    let p = fraction(bdd, low, terminals, memo).mean(fraction(bdd, high, terminals, memo));
    memo.insert(node, p);
    p
}

/// Writes the first satisfying path of `f` to `out`, which has room for the variables `0` to
/// `largest`, as `1` for true, `0` for false and `-1` for a variable the path does not decide.
/// Returns false, and leaves `out` unchanged, when `f` is false.
#[no_mangle]
pub unsafe extern "C" fn ruddy_pick_path(f: ruddy_bdd_t, largest: u32, out: *mut i8) -> bool {
    let m = unsafe { f.manager() };
    let Some(path) = m
        .satisfying_paths(unsafe { f.get() }, Some(VariableId::new(largest)))
        .next()
    else {
        return false;
    };
    let out = unsafe { std::slice::from_raw_parts_mut(out, path.len()) };
    for (o, v) in out.iter_mut().zip(path) {
        *o = match v {
            None => -1,
            Some(false) => 0,
            Some(true) => 1,
        };
    }
    true
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Builds the 4-queens problem through the C functions and checks its two solutions.
    #[test]
    fn queens_4() {
        unsafe {
            let m = ruddy_manager_new();
            let n = 4u32;
            let var = |i: u32, j: u32| i * n + j;
            let mut res = ruddy_true(m);
            let and_into = |res: &mut ruddy_bdd_t, g: ruddy_bdd_t| {
                let r = ruddy_and(*res, g);
                ruddy_bdd_free(*res);
                ruddy_bdd_free(g);
                *res = r;
            };
            for i in 0..n {
                // At least one queen in row i.
                let mut row = ruddy_false(m);
                for j in 0..n {
                    let x = ruddy_literal(m, var(i, j), true);
                    let r = ruddy_or(row, x);
                    ruddy_bdd_free(row);
                    ruddy_bdd_free(x);
                    row = r;
                }
                and_into(&mut res, row);
                // No two queens attack each other.
                for j in 0..n {
                    for k in 0..n {
                        for l in 0..n {
                            if (i, j) >= (k, l) {
                                continue;
                            }
                            let attack = i == k
                                || j == l
                                || (i as i64 - k as i64).abs() == (j as i64 - l as i64).abs();
                            if !attack {
                                continue;
                            }
                            let a = ruddy_literal(m, var(i, j), true);
                            let b = ruddy_literal(m, var(k, l), true);
                            let ab = ruddy_and(a, b);
                            let t = ruddy_true(m);
                            let nab = ruddy_and_not(t, ab);
                            for h in [a, b, ab, t] {
                                ruddy_bdd_free(h);
                            }
                            and_into(&mut res, nab);
                        }
                    }
                }
            }
            assert_eq!(ruddy_sat_count(res, n * n), 2.0);
            let mut path = vec![0i8; (n * n) as usize];
            assert!(ruddy_pick_path(res, n * n - 1, path.as_mut_ptr()));
            assert_eq!(path.iter().filter(|&&v| v == 1).count(), 4);
            ruddy_bdd_free(res);
            ruddy_manager_free(m);
        }
    }

    /// A conjunction of more literals than a double has exponent bits for has one model.
    #[test]
    fn deep_conjunction() {
        unsafe {
            let m = ruddy_manager_new();
            let n = 1300u32;
            let mut res = ruddy_true(m);
            for v in 0..n {
                let x = ruddy_literal(m, v, true);
                let r = ruddy_and(res, x);
                ruddy_bdd_free(res);
                ruddy_bdd_free(x);
                res = r;
            }
            assert_eq!(ruddy_sat_count(res, n), 1.0);
            assert_eq!(ruddy_sat_count(res, n + 10), 1024.0);
            let nres = ruddy_not(res);
            assert_eq!(ruddy_sat_count(nres, 20), 1048576.0);
            ruddy_bdd_free(res);
            ruddy_bdd_free(nres);
            ruddy_manager_free(m);
        }
    }

    #[test]
    fn and_not() {
        unsafe {
            let m = ruddy_manager_new();
            let a = ruddy_literal(m, 0, true);
            let b = ruddy_literal(m, 1, true);
            let d = ruddy_and_not(a, b);
            let nb = ruddy_not(b);
            let e = ruddy_and(a, nb);
            assert!(ruddy_bdd_eq(d, e));
            for h in [a, b, d, nb, e] {
                ruddy_bdd_free(h);
            }
            ruddy_manager_free(m);
        }
    }
}
