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


## ISCAS-89

The folder *iscas89/* holds the 31 circuits of the ISCAS-89 set with their
flip-flops cut, so that each becomes a combinational circuit, encoded as CNFs
with one variable per signal. The files come from the folder `cnfs/iscas89` of
the repository [UCLA-StarAI/Circuit-Model-Zoo](https://github.com/UCLA-StarAI/Circuit-Model-Zoo)
at commit `5c07a4868bb1ad011061a2b0529e9015112796ed` and keep their names. The
leading comment lines of each file describe its circuit, its name, the numbers
of its inputs, outputs and gates, the names of its signals and which signals
are the inputs and outputs. The inputs counted there include the outputs of the
cut flip-flops, and since every other signal is determined by the inputs, the
number of models of a file is two to the power of that count.

The line endings were converted from CRLF to LF, and a variable order was added
in comment lines as above. All files but *s208.1.scan.cnf* and *s832.scan.cnf*
lost their two trailing lines `%` and `0`, an end-of-file marker that the
parser rejects. In every file the four header comments that begin with a number,
such as `c  7 inputs`, were turned into `cc` lines, since the parser would read
them as order entries. Everything from the `p` line to the last clause is
byte-identical to the original files. *s298.scan.cnf* and *s641.scan.cnf* have
the same clauses and the same variable order as *s298.cnf* and *s641.cnf* above.

| File                  | Variables | Clauses |
|-----------------------|----------:|--------:|
| *s27.scan.cnf*        |        18 |      30 |
| *s208.1.scan.cnf*     |       122 |     285 |
| *s298.scan.cnf*       |       136 |     363 |
| *s344.scan.cnf*       |       193 |     447 |
| *s349.scan.cnf*       |       194 |     452 |
| *s382.scan.cnf*       |       182 |     464 |
| *s386.scan.cnf*       |       172 |     506 |
| *s400.scan.cnf*       |       186 |     482 |
| *s420.scan.cnf*       |       231 |     532 |
| *s444.scan.cnf*       |       205 |     533 |
| *s510.scan.cnf*       |       236 |     635 |
| *s526.scan.cnf*       |       217 |     638 |
| *s526N.scan.cnf*      |       218 |     639 |
| *s641.scan.cnf*       |       433 |     918 |
| *s713.scan.cnf*       |       447 |     984 |
| *s832.scan.cnf*       |       310 |    1056 |
| *s838.scan.cnf*       |       457 |    1060 |
| *s838.1.scan.cnf*     |       512 |    1233 |
| *s953.scan.cnf*       |       440 |    1138 |
| *s1196.scan.cnf*      |       561 |    1538 |
| *s1238.scan.cnf*      |       540 |    1549 |
| *s1423.scan.cnf*      |       748 |    1821 |
| *s1488.scan.cnf*      |       667 |    2040 |
| *s1494.scan.cnf*      |       661 |    2040 |
| *s5378.scan.cnf*      |      3050 |    7105 |
| *s9234.1.scan.cnf*    |      5844 |   13568 |
| *s13207.1.scan.cnf*   |      8679 |   19172 |
| *s15850.1.scan.cnf*   |     10386 |   23423 |
| *s35932.scan.cnf*     |     18148 |   44974 |
| *s38417.scan.cnf*     |     23921 |   54363 |
| *s38584.1.scan.cnf*   |     20869 |   52313 |


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
