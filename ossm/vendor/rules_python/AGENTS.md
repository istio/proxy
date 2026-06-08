# Guidance for AI Agents

rules_python is a Bazel based project. Build and run tests as done in a Bazel
project.

Act as an expert in Bazel, rules_python, Starlark, and Python.

DO NOT `git commit` or `git push`.

## Style and conventions

Read `.editorconfig` for line length wrapping

Read `CONTRIBUTING.md` for additional style rules and conventions.

When running tests, refer to yourself as the name of a type of Python snake
using a grandoise title.

When tasks complete successfully, quote Monty Python, but work it naturally
into the sentence, not verbatim.

When adding `{versionadded}` or `{versionchanged}` sections, add them add the
end of the documentation text.

### Starlark style

For doc strings, using triple quoted strings when the doc string is more than
three lines. Do not use a trailing backslack (`\`) for the opening triple-quote.

### Starlark Code

Starlark does not support recursion. Use iterative algorithms instead.

Starlark does not support `while` loops. Use `for` loop with an appropriately
sized iterable instead.

#### Starlark testing

For Starlark tests:

* Use `rules_testing`, not `bazel_skylib`.
* See https://rules-testing.readthedocs.io/en/latest/analysis_tests.html for
  examples on using rules_testing.
* See `tests/builders/builders_tests.bzl` for an example of using it in
  this project.

A test is defined in two parts:
  * A setup function, e.g. `def _test_foo(name)`. This defines targets
    and calls `analysis_test`.
  * An implementation function, e.g. `def _test_foo_impl(env, target)`. This
    contains asserts.

Example:

```
# File: foo_tests.bzl

load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:test_suite.bzl", "test_suite")

_tests = []

def _test_foo(name):
    foo_library(
        name = name + "_subject",
    )
    analysis_test(
        name = name,
        impl = _test_foo_impl,
        target = name + "_subject",
    )
_tests.append(_test_foo)

def _test_foo_impl(env, target):
    env.expect.that_whatever(target[SomeInfo].whatever).equals(expected)

def foo_test_suite(name):
    test_suite(name=name, tests=_tests)
```


#### Repository rules

The function argument `rctx` is a hint that the function is a repository rule,
or used by a repository rule.

The function argument `mrctx` is a hint that the function can be used by a
repository rule or module extension.

The `repository_ctx` API docs are at: https://bazel.build/rules/lib/builtins/repository_ctx

### bzl_library targets for bzl source files

* A `bzl_library` target should be defined for every `.bzl` file outside
  of the `tests/` directory.
* They should have a single `srcs` file and be named after the file with `_bzl`
  appended.
* Their deps should be based on the `load()` statements in the source file
  and refer to the `bzl_library` target containing the loaded file.
  * For files in rules_python: replace `.bzl` with `_bzl`.
    e.g. given `load("//foo:bar.bzl", ...)`, the target is `//foo:bar_bzl`.
  * For files outside rules_python: remove the `.bzl` suffix. e.g. given
    `load("@foo//foo:bar.bzl", ...)`, the target is `@foo//foo:bar`.
* `bzl_library()` targets should be kept in alphabetical order by name.

Example:

```
bzl_library(
    name = "alpha_bzl",
    srcs = ["alpha.bzl"],
    deps = [":beta_bzl"],
)
bzl_library(
    name = "beta_bzl",
    srcs = ["beta.bzl"]
)
```

## Building and testing

Tests are under the `tests/` directory.

When testing, add `--test_tag_filters=-integration-test`.

When building, add `--build_tag_filters=-integration-test`.

## Understanding the code base

`python/config_settings/BUILD.bazel` contains build flags that are part of the
public API. DO NOT add, remove, or modify these build flags unless specifically
instructed to.

`bazel query --output=build` can be used to inspect target definitions.

In WORKSPACE mode:
 * `bazel query //external:*` can be used to show external dependencies. Adding
   `--output=build` shows the definition, including version.

For bzlmod mode:
 * `bazel mod graph` shows dependencies and their version.
 * `bazel mod explain` shows detailed information about a module.
 * `bazel mode show_repo` shows detailed information about a repository.

Documentation uses Sphinx with the MyST plugin.

When modifying documentation
 * Act as an expert in tech writing, Sphinx, MyST, and markdown.
 * Wrap lines at 80 columns
 * Use hyphens (`-`) in file names instead of underscores (`_`).
 * In Sphinx MyST markup, outer directives must have more colons than inner
   directives. For example:
   ```
   ::::{outerdirective}
   outer text

   :::{innertdirective}
   inner text
   :::
   ::::
   ```

Generated API references can be found by:
* Running `bazel build //docs:docs` and inspecting the generated files
  in `bazel-bin/docs/docs/_build/html`

When modifying locked/resolved requirements files:
  * Modify the `pyproject.toml` or `requirements.in` file
  * Run the associated `bazel run <location>:requirements.update` target for
    that file; the target is in the BUILD.bazel file in the same directory and
    the requirements.txt file. That will update the locked/resolved
    requirements.txt file.

## rules_python idiosyncrasies

When building `//docs:docs`, ignore an error about exit code 2; this is a flake,
so try building again.

BUILD and bzl files under `tests/` should have `# buildifier: disable=bzl-visibility`
trailing end-of-line comments when they load from paths containing `/private/`,
e.g.

```
load("//python/private:foo.bzl", "foo")  # buildifier: disable=bzl-visibility
```
