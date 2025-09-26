# Integration tests for c++ CEL Runtime
## Benchmarks
To run the benchmark tests:

`blaze run -c opt --dynamic_mode=off //eval/tests:benchmark_test --benchmark_filter=all`

or

`blaze run -c opt --dynamic_mode=off //eval/tests:unknowns_benchmark_test --benchmark_filter=all`

see go/benchmark

For csv formatting: `awk '{print $1 "," $2 "," $3 "," $4}'`
