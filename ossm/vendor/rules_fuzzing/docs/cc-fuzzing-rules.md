<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Public definitions for fuzzing rules.

Definitions outside this file are private unless otherwise noted, and may
change without notice.

<a id="cc_fuzzing_engine"></a>

## cc_fuzzing_engine

<pre>
load("@rules_fuzzing//fuzzing:cc_defs.bzl", "cc_fuzzing_engine")

cc_fuzzing_engine(<a href="#cc_fuzzing_engine-name">name</a>, <a href="#cc_fuzzing_engine-display_name">display_name</a>, <a href="#cc_fuzzing_engine-launcher">launcher</a>, <a href="#cc_fuzzing_engine-launcher_data">launcher_data</a>, <a href="#cc_fuzzing_engine-library">library</a>)
</pre>

Specifies a fuzzing engine that can be used to run C++ fuzz targets.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="cc_fuzzing_engine-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="cc_fuzzing_engine-display_name"></a>display_name |  The name of the fuzzing engine, as it should be rendered in human-readable output.   | String | required |  |
| <a id="cc_fuzzing_engine-launcher"></a>launcher |  A shell script that knows how to launch the fuzzing executable based on configuration specified in the environment.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="cc_fuzzing_engine-launcher_data"></a>launcher_data |  A dict mapping additional runtime dependencies needed by the fuzzing engine to environment variables that will be available inside the launcher, holding the runtime path to the dependency.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: Label -> String</a> | optional |  `{}`  |
| <a id="cc_fuzzing_engine-library"></a>library |  A cc_library target that implements the fuzzing engine entry point.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |


<a id="FuzzingEngineInfo"></a>

## FuzzingEngineInfo

<pre>
load("@rules_fuzzing//fuzzing:cc_defs.bzl", "FuzzingEngineInfo")

FuzzingEngineInfo(<a href="#FuzzingEngineInfo-display_name">display_name</a>, <a href="#FuzzingEngineInfo-launcher">launcher</a>, <a href="#FuzzingEngineInfo-launcher_runfiles">launcher_runfiles</a>, <a href="#FuzzingEngineInfo-launcher_environment">launcher_environment</a>)
</pre>

Provider for storing the language-independent part of the specification of a fuzzing engine.

**FIELDS**

| Name  | Description |
| :------------- | :------------- |
| <a id="FuzzingEngineInfo-display_name"></a>display_name |  A string representing the human-readable name of the fuzzing engine.    |
| <a id="FuzzingEngineInfo-launcher"></a>launcher |  A file representing the shell script that launches the fuzz target.    |
| <a id="FuzzingEngineInfo-launcher_runfiles"></a>launcher_runfiles |  The runfiles needed by the launcher script on the fuzzing engine side, such as helper tools and their data dependencies.    |
| <a id="FuzzingEngineInfo-launcher_environment"></a>launcher_environment |  A dictionary from environment variables to files used by the launcher script.    |


<a id="cc_fuzz_test"></a>

## cc_fuzz_test

<pre>
load("@rules_fuzzing//fuzzing:cc_defs.bzl", "cc_fuzz_test")

cc_fuzz_test(<a href="#cc_fuzz_test-name">name</a>, <a href="#cc_fuzz_test-corpus">corpus</a>, <a href="#cc_fuzz_test-dicts">dicts</a>, <a href="#cc_fuzz_test-engine">engine</a>, <a href="#cc_fuzz_test-size">size</a>, <a href="#cc_fuzz_test-tags">tags</a>, <a href="#cc_fuzz_test-timeout">timeout</a>, <a href="#cc_fuzz_test-binary_kwargs">**binary_kwargs</a>)
</pre>

Defines a C++ fuzz test and a few associated tools and metadata.

For each fuzz test `<name>`, this macro defines a number of targets. The
most relevant ones are:

* `<name>`: A test that executes the fuzzer binary against the seed corpus
  (or on an empty input if no corpus is specified).
* `<name>_bin`: The instrumented fuzz test executable. Use this target
  for debugging or for accessing the complete command line interface of the
  fuzzing engine. Most developers should only need to use this target
  rarely.
* `<name>_run`: An executable target used to launch the fuzz test using a
  simpler, engine-agnostic command line interface.
* `<name>_oss_fuzz`: Generates a `<name>_oss_fuzz.tar` archive containing
  the fuzz target executable and its associated resources (corpus,
  dictionary, etc.) in a format suitable for unpacking in the $OUT/
  directory of an OSS-Fuzz build. This target can be used inside the
  `build.sh` script of an OSS-Fuzz project.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="cc_fuzz_test-name"></a>name |  A unique name for this target. Required.   |  none |
| <a id="cc_fuzz_test-corpus"></a>corpus |  A list containing corpus files.   |  `None` |
| <a id="cc_fuzz_test-dicts"></a>dicts |  A list containing dictionaries.   |  `None` |
| <a id="cc_fuzz_test-engine"></a>engine |  A label pointing to the fuzzing engine to use.   |  `Label("@rules_fuzzing//fuzzing:cc_engine")` |
| <a id="cc_fuzz_test-size"></a>size |  The size of the regression test. This does *not* affect fuzzing itself. Takes the [common size values](https://bazel.build/reference/be/common-definitions#test.size).   |  `None` |
| <a id="cc_fuzz_test-tags"></a>tags |  Tags set on the regression test.   |  `None` |
| <a id="cc_fuzz_test-timeout"></a>timeout |  The timeout for the regression test. This does *not* affect fuzzing itself. Takes the [common timeout values](https://docs.bazel.build/versions/main/be/common-definitions.html#test.timeout).   |  `None` |
| <a id="cc_fuzz_test-binary_kwargs"></a>binary_kwargs |  Keyword arguments directly forwarded to the fuzz test binary rule.   |  none |


<a id="fuzzing_decoration"></a>

## fuzzing_decoration

<pre>
load("@rules_fuzzing//fuzzing:cc_defs.bzl", "fuzzing_decoration")

fuzzing_decoration(<a href="#fuzzing_decoration-name">name</a>, <a href="#fuzzing_decoration-raw_binary">raw_binary</a>, <a href="#fuzzing_decoration-engine">engine</a>, <a href="#fuzzing_decoration-corpus">corpus</a>, <a href="#fuzzing_decoration-dicts">dicts</a>, <a href="#fuzzing_decoration-instrument_binary">instrument_binary</a>,
                   <a href="#fuzzing_decoration-define_regression_test">define_regression_test</a>, <a href="#fuzzing_decoration-test_size">test_size</a>, <a href="#fuzzing_decoration-test_tags">test_tags</a>, <a href="#fuzzing_decoration-test_timeout">test_timeout</a>)
</pre>

Generates the standard targets associated to a fuzz test.

This macro can be used to define custom fuzz test rules in case the default
`cc_fuzz_test` macro is not adequate. Refer to the `cc_fuzz_test` macro
documentation for the set of targets generated.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="fuzzing_decoration-name"></a>name |  The name prefix of the generated targets. It is normally the fuzz test name in the BUILD file.   |  none |
| <a id="fuzzing_decoration-raw_binary"></a>raw_binary |  The label of the cc_binary or cc_test of fuzz test executable.   |  none |
| <a id="fuzzing_decoration-engine"></a>engine |  The label of the fuzzing engine used to build the binary.   |  none |
| <a id="fuzzing_decoration-corpus"></a>corpus |  A list of corpus files.   |  `None` |
| <a id="fuzzing_decoration-dicts"></a>dicts |  A list of fuzzing dictionary files.   |  `None` |
| <a id="fuzzing_decoration-instrument_binary"></a>instrument_binary |  **(Experimental, may be removed in the future.)**<br><br>By default, the generated targets depend on `raw_binary` through a Bazel configuration using flags from the `@rules_fuzzing//fuzzing` package to determine the fuzzing build mode, engine, and sanitizer instrumentation.<br><br>When this argument is false, the targets assume that `raw_binary` is already built in the proper configuration and will not apply the transition.<br><br>Most users should not need to change this argument. If you think the default instrumentation mode does not work for your use case, please file a Github issue to discuss.   |  `True` |
| <a id="fuzzing_decoration-define_regression_test"></a>define_regression_test |  If true, generate a regression test rule.   |  `True` |
| <a id="fuzzing_decoration-test_size"></a>test_size |  The size of the fuzzing regression test.   |  `None` |
| <a id="fuzzing_decoration-test_tags"></a>test_tags |  Tags set on the fuzzing regression test.   |  `None` |
| <a id="fuzzing_decoration-test_timeout"></a>test_timeout |  The timeout for the fuzzing regression test.   |  `None` |


