Unit Tests
==========
This directory contains the unit tests for dd-trace-cpp.

The testing library used is [Catch2][1], vendored here as a single header,
[catch.hpp](catch.cpp) (see the [Makefile](Makefile)).

[../bin/test](../bin/test) builds and runs the unit tests.

Code Layout
-----------
Test-specific implementations of interfaces are defined in [mocks/](mocks).

[test.h](test.h) is a wrapper around [catch.hpp](catch.hpp).

[matchers.h](matchers.h) defines extensions to Catch2 that are convenient in
test assertions.

[main.cpp](main.cpp) is the test driver (executable).

All other translation units in this directory are the tests themselves.  For
example, [test_span.cpp](test_span.cpp) contains the tests for the `Span` class
and associated behavior.

[1]: https://github.com/catchorg/Catch2/tree/v2.x
