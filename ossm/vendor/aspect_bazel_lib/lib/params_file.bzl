"params_file public API"

load("//lib/private:params_file.bzl", _params_file = "params_file")

def params_file(
        name,
        out,
        args = [],
        data = [],
        newline = "auto",
        **kwargs):
    """Generates a UTF-8 encoded params file from a list of arguments.

    Handles variable substitutions for args.

    Args:
        name: Name of the rule.
        out: Path of the output file, relative to this package.
        args: Arguments to concatenate into a params file.

            - Subject to 'Make variable' substitution. See https://docs.bazel.build/versions/main/be/make-variables.html.

            - Subject to predefined source/output path variables substitutions.

              The predefined variables `execpath`, `execpaths`, `rootpath`, `rootpaths`, `location`, and `locations` take
              label parameters (e.g. `$(execpath //foo:bar)`) and substitute the file paths denoted by that label.

              See https://docs.bazel.build/versions/main/be/make-variables.html#predefined_label_variables for more info.

              NB: This $(location) substitution returns the manifest file path which differs from the `*_binary` & `*_test`
              args and genrule bazel substitutions. This will be fixed in a future major release.
              See docs string of `expand_location_into_runfiles` macro in `internal/common/expand_into_runfiles.bzl`
              for more info.

            - Subject to predefined variables & custom variable substitutions.

              Predefined "Make" variables such as `$(COMPILATION_MODE)` and `$(TARGET_CPU)` are expanded.
              See https://docs.bazel.build/versions/main/be/make-variables.html#predefined_variables.

              Custom variables are also expanded including variables set through the Bazel CLI with `--define=SOME_VAR=SOME_VALUE`.
              See https://docs.bazel.build/versions/main/be/make-variables.html#custom_variables.

              Predefined genrule variables are not supported in this context.

        data: Data for `$(location)` expansions in args.
        newline: Line endings to use. One of [`"auto"`, `"unix"`, `"windows"`].

            - `"auto"` for platform-determined
            - `"unix"` for LF
            - `"windows"` for CRLF

        **kwargs: undocumented named arguments
    """
    _params_file(
        name = name,
        out = out,
        args = args,
        data = data,
        newline = newline or "auto",
        **kwargs
    )
