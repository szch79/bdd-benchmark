# CNF for Bucket Elimination (*.cnf*)

Inputs of the CNF benchmark with the option `-b`. They are the bucket elimination benchmarks of
the artifact of

> Randal E. Bryant. "TBUDDY: A Proof-Generating BDD Package". In: *Formal Methods in
> Computer-Aided Design (FMCAD)*. 2022

at https://github.com/rebryant/tbuddy-artifact (MIT license, copyright 2022 Randal Bryant), its
files `benchmarks/files/*.cnf` at the sizes the artifact runs. All of them are unsatisfiable.

- *chew-heule-N*: an odd and an even parity constraint over the same *N* variables, each
  encoded with auxiliary variables, the second in a random permutation (Chew and Heule 2020).
- *urquhart-li-M*: Urquhart's parity formulas (Urquhart 1987) over a bipartite graph with
  2*M*^2 nodes, from Li's generator (Li 2003).

## Bucket Elimination

Each term, initially a clause, is kept in the bucket of the variable at the root of its BDD. The
buckets are processed in the variable order. The terms of a bucket are conjoined in pairs, in the
order they were added, and each product is added to the bucket of its root variable, which may be
the current one. The last term of the bucket is existentially quantified over the bucket's
variable and added to the bucket of its root variable unless it is true. The formula is
unsatisfiable once a term is false, and satisfiable once all buckets are empty.
