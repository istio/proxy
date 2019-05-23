# Opencensus Library

This folder includes opencensus library copied from <https://github.com/census-instrumentation/opencensus-cpp> with customization. Even though opencensus-cpp repo has already had bazel setup, the original code won't work out of box within a wasm sandbox or in an envoy worker silo, so we have to copy and customize it.

The library is mainly used to do data aggregation for some predefined metrics and export them to Stackdriver. The code in this directory should mostly remain unchanged unless bug is found.