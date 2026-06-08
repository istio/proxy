W3C Propagation Fuzzer
======================
This directory defines an executable, `fuzz`, that fuzz tests extraction and
injection of the W3C tracing HTTP headers "traceparent" and "tracestate".

Libfuzzer invokes the `LLVMFuzzerTestOneInput` function repeatedly with a binary
blob of varying size and contents.  For each blob, [fuzz.cpp](./fuzz.cpp) runs
its test multiple times. The input blob is interpreted in the following way:
```text
blob:          _ _ _ _ _ _ _ _ _

iteration 0:   s s s s s s s s s

iteration 1:   p s s s s s s s s

iteration 2:   p p s s s s s s s

iteration 3:   p p p s s s s s s

...

iteration 8:   p p p p p p p p s

iteration 9:   p p p p p p p p p
```
 The `p`-labeled bytes are the "traceparent" header value, while the `s`-labeled
 bytes are the "tracestate" header value. So, for an input blob of length `n`,
 the `LLVMFuzzerTestOneInput` runs its test `n + 1` times, for the `n + 1`
 possible divisions of the input between "traceparent" and "tracestate".

 Each test uses a singleton[^1] `Tracer` to `extract_span` from a `DictReader`
 containing the "traceparent" and "tracestate" headers. If that succeeds, then
 the test `inject`s the resulting span into a no-op `DictWriter`.

[^1]: thread-local, actually, though it doesn't matter because even libfuzzer's
  "worker" mode forks instead of threads
