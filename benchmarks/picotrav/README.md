# Picotrav (*.blif*)

With *not_a.blif* and *not_b.blif* we provide two gates that both implement
`~[0]` in two different ways. Furthermore, *cyclic_nets.blif* is an invalid
input to test cycle detection as part of input validation.

## EPFL (v2023.1)

The folder *epfl2023/* holds circuits of the EPFL combinational benchmark suite
of Amarú, Gaillardon and De Micheli ("The EPFL Combinational Benchmark Suite",
IWLS 2015), copied byte-identical from the tag `v2023.1` of the repository
[lsils/benchmarks](https://github.com/lsils/benchmarks), at commit
`7770275a0e07a27b8ea9f65b6a3f767282fb8226`, under their paths in that
repository. The repository is distributed under the MIT licence, which
*epfl2023/LICENSE* reproduces.

The folder holds ten original circuits, *arithmetic/adder.blif* and the nine
circuits *arbiter*, *cavlc*, *ctrl*, *dec*, *i2c*, *int2float*, *mem_ctrl*,
*priority* and *router* in *random_control/*, and for each of them its best
results of that tag for depth, in *best_results/depth/*, and for size, in
*best_results/size/*, optimised circuits that implement the same function.
*ctrl_depth_2023.blif* and *ctrl_size_2023.blif* are the same file.

| Circuit     | Best result for depth       | Best result for size       | Order      |
|-------------|-----------------------------|----------------------------|------------|
| *adder*     | *adder_depth_2023.blif*     | *adder_size_2022.blif*     | `level_df` |
| *arbiter*   | *arbiter_depth_2022.blif*   | *arbiter_size_2023.blif*   | `level_df` |
| *cavlc*     | *cavlc_depth_2022.blif*     | *cavlc_size_2023.blif*     | `input`    |
| *ctrl*      | *ctrl_depth_2023.blif*      | *ctrl_size_2023.blif*      | `input`    |
| *dec*       | *dec_depth_2018.blif*       | *dec_size_2018.blif*       | `input`    |
| *i2c*       | *i2c_depth_2023.blif*       | *i2c_size_2023.blif*       | `input`    |
| *int2float* | *int2float_depth_2022.blif* | *int2float_size_2023.blif* | `input`    |
| *mem_ctrl*  | *mem_ctrl_depth_2023.blif*  | *mem_ctrl_size_2023.blif*  | `level_df` |
| *priority*  | *priority_depth_2022.blif*  | *priority_size_2023.blif*  | `input`    |
| *router*    | *router_depth_2022.blif*    | *router_size_2022.blif*    | `input`    |

The pairs of an original circuit and one of its best results are those that
the harness's own instance list *slurm/make.py* runs on BDDs, and that Husung,
Dubslaff, Hermanns and Köhl run in "OxiDD: A Safe, Concurrent, Modular, and
Performant Decision Diagram Framework in Rust" (TACAS 2024), except for the
circuits *voter* and *sin*, whose runs take hours. The last column of the table
is the variable order that *slurm/make.py* gives the circuit.

## More Inputs

- EPFL Combinatorial Benchmark Suite
  - https://github.com/lsils/benchmarks

- Nanotrav examples of the *CUDD* package (_.blif_)
  - *external/cudd/nanotrav/*

- Hardware Model Checking Competition (_.aig_)
  - HWMCC'07: http://fmv.jku.at/hwmcc07/
  - HWMCC'08: http://fmv.jku.at/hwmcc08/
  - HWMCC'10: http://fmv.jku.at/hwmcc10/
  - HWMCC'11: http://fmv.jku.at/hwmcc11/
  - HWMCC'12: http://fmv.jku.at/hwmcc12/
  - HWMCC'13: http://fmv.jku.at/hwmcc13/
