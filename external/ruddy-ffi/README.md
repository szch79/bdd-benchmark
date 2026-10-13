# C FFI for sybila/ruddy

A minimal C foreign function interface for the shared representation of [ruddy](https://github.com/sybila/ruddy), to integrate this BDD library into [BDD Benchmark](https://github.com/SSoelvsten/bdd-benchmark). It exposes only what the benchmarks use and is not meant for other code.

The library does not bound its node table, so the interface takes a budget of table entries when a manager is created and aborts the process when an operation leaves more entries than that, even after a garbage collection, as the FFI for LibBDD does.
