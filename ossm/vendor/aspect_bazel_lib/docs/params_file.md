<!-- Generated with Stardoc: http://skydoc.bazel.build -->

params_file public API

<a id="params_file"></a>

## params_file

<pre>
params_file(<a href="#params_file-name">name</a>, <a href="#params_file-out">out</a>, <a href="#params_file-args">args</a>, <a href="#params_file-data">data</a>, <a href="#params_file-newline">newline</a>, <a href="#params_file-kwargs">kwargs</a>)
</pre>

Generates a UTF-8 encoded params file from a list of arguments.

Handles variable substitutions for args.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="params_file-name"></a>name |  Name of the rule.   |  none |
| <a id="params_file-out"></a>out |  Path of the output file, relative to this package.   |  none |
| <a id="params_file-args"></a>args |  Arguments to concatenate into a params file.<br><br>- Subject to 'Make variable' substitution. See https://docs.bazel.build/versions/main/be/make-variables.html.<br><br>- Subject to predefined source/output path variables substitutions.<br><br>  The predefined variables `execpath`, `execpaths`, `rootpath`, `rootpaths`, `location`, and `locations` take   label parameters (e.g. `$(execpath //foo:bar)`) and substitute the file paths denoted by that label.<br><br>  See https://docs.bazel.build/versions/main/be/make-variables.html#predefined_label_variables for more info.<br><br>  NB: This $(location) substitution returns the manifest file path which differs from the `*_binary` & `*_test`   args and genrule bazel substitutions. This will be fixed in a future major release.   See docs string of `expand_location_into_runfiles` macro in `internal/common/expand_into_runfiles.bzl`   for more info.<br><br>- Subject to predefined variables & custom variable substitutions.<br><br>  Predefined "Make" variables such as `$(COMPILATION_MODE)` and `$(TARGET_CPU)` are expanded.   See https://docs.bazel.build/versions/main/be/make-variables.html#predefined_variables.<br><br>  Custom variables are also expanded including variables set through the Bazel CLI with `--define=SOME_VAR=SOME_VALUE`.   See https://docs.bazel.build/versions/main/be/make-variables.html#custom_variables.<br><br>  Predefined genrule variables are not supported in this context.   |  `[]` |
| <a id="params_file-data"></a>data |  Data for `$(location)` expansions in args.   |  `[]` |
| <a id="params_file-newline"></a>newline |  Line endings to use. One of [`"auto"`, `"unix"`, `"windows"`].<br><br>- `"auto"` for platform-determined - `"unix"` for LF - `"windows"` for CRLF   |  `"auto"` |
| <a id="params_file-kwargs"></a>kwargs |  undocumented named arguments   |  none |


