# Python Rules for Bazel

[![Build status](https://badge.buildkite.com/0bcfe58b6f5741aacb09b12485969ba7a1205955a45b53e854.svg?branch=main)](https://buildkite.com/bazel/rules-python-python)

## Overview

This repository is the home of the core Python rules -- `py_library`,
`py_binary`, `py_test`, and related symbols that provide the basis for Python
support in Bazel. It also contains package installation rules for integrating with PyPI and other indices. 

Documentation for rules_python is at <https://rules-python.readthedocs.io> and in the
[Bazel Build Encyclopedia](https://docs.bazel.build/versions/master/be/python.html).

Examples live in the [examples](examples) directory.

The core rules are stable. Their implementation is subject to Bazel's
[backward compatibility policy](https://docs.bazel.build/versions/master/backward-compatibility.html).
This repository aims to follow [semantic versioning](https://semver.org).

The Bazel community maintains this repository. Neither Google nor the Bazel team provides support for the code. However, this repository is part of the test suite used to vet new Bazel releases. See [How to contribute](CONTRIBUTING.md) page for information on our development workflow.

## Documentation

For detailed documentation, see <https://rules-python.readthedocs.io>

## Bzlmod support

See [Bzlmod support](BZLMOD_SUPPORT.md) for more details.
