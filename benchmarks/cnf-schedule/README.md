# CNF with Schedules (*.cnf* and *.schedule*)

Inputs of the CNF benchmark with the option `-s`, each formula *F.cnf* with its schedule
*F.schedule*. They are the scanning benchmarks of the artifact of

> Randal E. Bryant. "TBUDDY: A Proof-Generating BDD Package". In: *Formal Methods in
> Computer-Aided Design (FMCAD)*. 2022

at https://github.com/rebryant/tbuddy-artifact (MIT license, copyright 2022 Randal Bryant),
generated at the sizes the artifact runs. All of them are unsatisfiable.

- *chess-scan-N*: tiling the mutilated *N* x *N* chessboard with dominoes, from
  `python3 benchmarks/generators/chess.py -n N -r chess-scan-N`.
- *pigeon-scan-N*: placing *N*+1 pigeons into *N* holes, the at-most-one constraints encoded
  with auxiliary variables, from `python3 benchmarks/generators/pigeon-sinz.py -n N -r
  pigeon-scan-N`.

The generators also write a variable order *F.order*, which the artifact does not use, so the
variable order is the identity.

## Schedules

A schedule is a stack program over terms, each a BDD for a set of clauses with some variables
existentially quantified. Each line holds one command.

- `c i j ...` pushes the clauses numbered *i*, *j*, ..., counted from 1 in the order of the file.
- `a k` pops a term and conjoins it with the *k* terms below it, one after the other.
- `q x y ...` quantifies the variables *x*, *y*, ... in the term on top of the stack.
- Lines starting with `#` or `i` are comments.

The schedule may push each clause at most once. If it leaves more than one term on the stack, the
remaining terms are combined by bucket elimination, as with the option `-b`. The commands `=` and
`g` of the artifact's solver `tbsat`, which work on parity constraints, are not supported.
