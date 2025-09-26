Microbenchmarks
===============
This directory contains the definition of a program that measures the timing and
resource consumption of a test tracing scenario.

The benchmark uses [Google Benchmark][1], whose source is included as a git
submodule under `./google-benchmark`.

The scenario that's measured is similar to the [../example][3] setup.  A trace
is created whose structure reflects that of a particular file directory
structure.  The directory structure, in this case, is the source tree of the
[Tiny C Compiler][4], whose source is included as a git submodule under
`./tinycc`.

The scenario does not use the network, spawn any threads, or read/write
any files. The operations that are implicitly covered by the scenario are:

- configuring and initializing a tracer,
- creating a trace,
- adding spans to a trace,
- setting tags on spans,
- finishing spans,
- finalizing a trace and making a sampling decision,
- serializing a trace as MessagePack.

[../bin/benchmark][6] is a script that builds dd-trace-cpp, this benchmark, and
then runs the benchmark.

This benchmark is intended to be driven by Datadog's internal benchmarking
platform.  See [../.gitlab/benchmarks.yml][7].

[1]: https://github.com/google/benchmark
[3]: ../example
[4]: https://bellard.org/tcc/
[6]: ../bin/benchmark
[7]: ../.gitlab/benchmarks.yml
