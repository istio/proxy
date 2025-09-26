# Rules Rust - Architecture

In this file we describe how we think about the architecture
of `rules_rust`. Its goal is to help contributors orient themselves, and to
document code restrictions and assumptions.

In general we try to follow the common standard defined by
https://docs.bazel.build/versions/master/skylark/deploying.html.

## //rust

This is the core package of the rules. It contains all the core rules such as
`rust_binary` and `rust_library`. It also contains `rust_common`, a Starlark
struct providing all rules_rust APIs that one might need when writing custom
rules integrating with rules_rust.

`//rust` and all its subpackages have to be standalone. Users who only need
the core rules should be able to ignore all other packages.

`//rust:defs.bzl` is the file that all users of `rules_rust` will be using.
Everything in this file can be used (depended on) and is supported  (though
stability is not currently guaranteed across commits, see
[#600](https://github.com/bazelbuild/rules_rust/issues/600)). Typically this
file re-exports definitions from other files, typically from `//rust/private`.
Also other packages in `rules_rust` should access core rules through the public
API only. We expect dogfooding our own APIs increases their
quality.

`//rust/private` package contains code for rule implementation. This file can only
depend on `//rust` and its subpackages. Exceptions are Bazel's builtin packages
and public APIs of other rules (such as `rules_cc`). Depending on
`//rust/private` from packages other than `//rust` is not supported, we reserve
the right to change anything there and not tell anybody.

When core rules need custom tools (such as process wrapper, launcher, test
runner, and so on), they should be stored in `//rust/tools` (for public tools)
or in `//rust/private/tools` (for private tools). These should:

* be essential (it does not make sense to have core rules without them)
* have few or no third party dependencies, and their third party dependencies
    should be checked in.
* be usable for cross compilation
* have only essential requirements on the host system (requiring that a custom
    library is installed on the system is frowned upon)

## //examples (@examples)

Examples package is actually a local repository. This repository can have
additional dependencies on anything that helps demonstrate an example. Non
trivial examples should have an accompanying README.md file.

The goal of examples is to demonstrate usage of `rules_rust`, not to test code.
Use `//test` for testing.

## //test

Contains unit (in `//test/unit`) and integration tests. CI configuration is
stored in .bazelci.
