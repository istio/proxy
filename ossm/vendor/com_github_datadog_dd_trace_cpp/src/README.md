Source Code
===========
This directory contains the dd-trace-cpp library code, including all header
files.

The source and headers are together under [datadog/](datadog/).  The intended
inclusion style for client code is
```c++
#include <datadog/some_header_file.h>
```

No distinction is made between the public API and implementation details.

Each header file contains a comment describing the component's purpose and
intended use.
