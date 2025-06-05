Datadog C++ Tracing Library
===========================
[![codecov](https://codecov.io/gh/DataDog/dd-trace-cpp/graph/badge.svg?token=78VYILWPMC)](https://codecov.io/gh/DataDog/dd-trace-cpp)

```c++
#include <datadog/span_config.h>
#include <datadog/tracer.h>
#include <datadog/tracer_config.h>

#include <chrono>
#include <iostream>
#include <thread>

int main() {
    namespace dd = datadog::tracing;

    dd::TracerConfig config;
    config.service = "my-service";

    const auto validated_config = dd::finalize_config(config);
    if (!validated_config) {
        std::cerr << validated_config.error() << '\n';
        return 1;
    }

    dd::Tracer tracer{*validated_config};
    dd::SpanConfig options;

    options.name = "parent";
    dd::Span parent = tracer.create_span(options);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    options.name = "child";
    dd::Span child = parent.create_child(options);
    child.set_tag("foo", "bar");

    std::this_thread::sleep_for(std::chrono::seconds(2));
}
```
See the [examples](examples) directory for more extensive usage examples.

## Platform Support
The library has been tested and is compatible on the following CPU architecture, OS and compiler combinations:
- x86_64 and arm64 Linux with GCC 11.4.
- x86_64 and arm64 Linux with Clang 14.
- x86_64 Windows with MSVC 2022.
- arm64 macOS with Apple Clang 15.


## Building and Installation

### Requirements
`dd-trace-cpp` requires a [supported](#platform-support) C++17 compiler.

A recent version of CMake is required (`3.24`), which might not be in your
system's package manager. [bin/install-cmake](bin/install-cmake) is an installer
for a recent CMake.

### Building
Build this library from source using [CMake][1].

```shell
git clone 'https://github.com/datadog/dd-trace-cpp'
cd dd-trace-cpp
cmake -B build .
cmake --build build -j
```

By default CMake will generate both static and shared libraries. To build either on of them use
either `BUILD_SHARED_LIBS` or `BUILD_STATIC_LIBS`. Example:

```shell
cmake -B build -DBUILD_SHARED_LIBS=1 .
```

### Installation
Installation places a shared library and public headers into the appropriate system directories
(`/usr/local/[...]`), or to a specified installation prefix.

```shell
cmake --install

# Here is how to install dd-trace-cpp into `.install/` within the source
# repository.
# cmake --install build --prefix=.install
```

### Optional: Linking to the shared library
In case you decided to build the shared library:

When building an executable that uses `dd-trace-cpp`, specify the path to
the installed headers using an appropriate `-I` option.  If the library was
installed into the default system directories, then the `-I` option is not
needed.
```shell
c++ -I/path/to/dd-trace-cpp/.install/include -c -o my_app.o my_app.cpp
```

When linking an executable that uses `dd-trace-cpp`, specify linkage to the
built library using the `-ldd_trace_cpp` option and an appropriate `-L` option.
If the library was installed into the default system directories, then the `-L`
options is not needed. The `-ldd_trace_cpp` option is always needed.
```shell
c++ -o my_app my_app.o -L/path/to/dd-trace-cpp/.install/lib -ldd_trace_cpp
```

Test
----
Pass `-DDD_TRACE_BUILD_TESTING=1` to `cmake` to include the unit tests in the build.

The resulting unit test executable is `test/tests` within the build directory.
```shell
cmake -Bbuild -DDD_TRACE_BUILD_TESTING=1 ..
cmake --build build -j
./build/test/tests
```

Alternatively, [bin/test](bin/test) is provided for convenience.

Code coverage reports are available [here][2].

Contributing
------------
See the [contributing guidelines](CONTRIBUTING.md).

[1]: https://cmake.org/
[2]: https://datadog.github.io/dd-trace-cpp-coverage
