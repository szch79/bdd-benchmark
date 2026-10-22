# CNF

The CNF benchmark accepts inputs in DIMACS CNF format. With *sample.cnf*, we
provide a small example to illustrate how the format works:

```
c 2 b
c 1 a
c 3 c
c 4 d
p cnf 4 3
-1 2 0
-2 1 0
3 0
```

The comment lines (starting with `c`) specify the variable order. Here,
variable 2 called “b” is at the top, then follows variable 1 “a”, and so on.
Note that variables are numbered from 1 to *n* (inclusively). Specifying the
variable order like this is optional, otherwise variable 1 is at the top and
variable *n* is at the bottom.

The line `p cnf 4 3` indicates that the CNF has 4 variables and 3 clauses. The
clauses follow on the next lines and are separated by `0`. In principle, they
could span more than one line or multiple clauses could be given on one line.
The CNF here is (¬x<sub>1</sub> ∨ x<sub>2</sub>) ∧ (¬x<sub>2</sub> ∨ x<sub>1</sub>) ∧ x<sub>3</sub>.


## Circuits

The files *c8.cnf*, *count.cnf*, *s298.cnf* and *s641.cnf* encode benchmark
circuits as CNFs, *s298* and *s641* from the ISCAS-89 set, with a variable order
added in comment lines as above. *s298.cnf* also keeps a description of its
circuit in its leading comment lines, its name, the numbers of its inputs,
outputs and gates, and the names of its signals.

| File         | Variables | Clauses |
|--------------|----------:|--------:|
| *c8.cnf*     |       227 |     713 |
| *count.cnf*  |       185 |     425 |
| *s298.cnf*   |       136 |     363 |
| *s641.cnf*   |       433 |     918 |


## More Inputs

- http://www.cril.univ-artois.fr/kc/benchmarks.html
- https://zenodo.org/records/10449477
- https://zenodo.org/records/10303558
- https://zenodo.org/records/10031810
- https://zenodo.org/records/10012857
- https://zenodo.org/records/10012860
- https://zenodo.org/records/10012864

It makes sense to preprocess raw CNF files using external tools (e.g.
[pmc](http://www.cril.univ-artois.fr/KC/pmc.html)) and infer good variable and
clause orders (e.g. using [MINCE](http://www.aloul.net/Tools/mince/)). The tool
[dimagic](https://zenodo.org/records/12707100) automates this.
